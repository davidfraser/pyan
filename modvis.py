#!/usr/bin/env python3
# -*- coding: utf-8; -*-
"""A simple import analyzer. Visualize dependencies between modules."""

import ast
from glob import glob
import logging
from optparse import OptionParser  # TODO: migrate to argparse
import os

import pyan.node
import pyan.visgraph
import pyan.writers

# from pyan.anutils import get_module_name


def filename_to_module_name(fullpath):  # we need to see __init__, hence we don't use anutils.get_module_name.
    """'some/path/module.py' -> 'some.path.module'"""
    if not fullpath.endswith(".py"):
        raise ValueError("Expected a .py filename, got '{}'".format(fullpath))
    rel = ".{}".format(os.path.sep)  # ./
    if fullpath.startswith(rel):
        fullpath = fullpath[len(rel) :]
    fullpath = fullpath[:-3]  # remove .py
    return fullpath.replace(os.path.sep, ".")


def split_module_name(m):
    """'fully.qualified.name' -> ('fully.qualified', 'name')"""
    k = m.rfind(".")
    if k == -1:
        return ("", m)
    return (m[:k], m[(k + 1) :])


# blacklist = (".git", "build", "dist", "test")
# def find_py_files(basedir):
#     py_files = []
#     for root, dirs, files in os.walk(basedir):
#         for x in blacklist:  # don't visit blacklisted dirs
#             if x in dirs:
#                 dirs.remove(x)
#         for filename in files:
#             if filename.endswith(".py"):
#                 fullpath = os.path.join(root, filename)
#                 py_files.append(fullpath)
#     return py_files


def resolve(current_module, target_module, level):
    """Return fully qualified name of the target_module in an import.

    If level == 0, the import is absolute, hence target_module is already the
    fully qualified name (and will be returned as-is).

    Relative imports (level > 0) are resolved using current_module as the
    starting point. Usually this is good enough (especially if you analyze your
    project by invoking modvis in its top-level directory).

    For the exact implications, see the section "Import sibling packages" in:
        https://alex.dzyoba.com/blog/python-import/
    and this SO discussion:
        https://stackoverflow.com/questions/14132789/relative-imports-for-the-billionth-time
    """
    if level < 0:
        raise ValueError("Relative import level must be >= 0, got {}".format(level))
    if level == 0:  # absolute import
        return target_module
    # level > 0 (let's have some simplistic support for relative imports)
    if level > current_module.count(".") + 1:  # foo.bar.baz -> max level 3, pointing to top level
        raise ValueError("Relative import level {} too large for module name {}".format(level, current_module))
    base = current_module
    for _ in range(level):
        k = base.rfind(".")
        if k == -1:
            base = ""
            break
        base = base[:k]
    return ".".join((base, target_module))


class ImportVisitor(ast.NodeVisitor):
    def __init__(self, filenames, logger):
        self.modules = {}  # modname: {dep0, dep1, ...}
        self.fullpaths = {}  # modname: fullpath
        self.logger = logger
        self.analyze(filenames)

    def analyze(self, filenames):
        for fullpath in filenames:
            with open(fullpath, "rt", encoding="utf-8") as f:
                content = f.read()
            m = filename_to_module_name(fullpath)
            self.current_module = m
            self.fullpaths[m] = fullpath
            self.visit(ast.parse(content, fullpath))

    def add_dependency(self, target_module):  # source module is always self.current_module
        m = self.current_module
        if m not in self.modules:
            self.modules[m] = set()
        self.modules[m].add(target_module)
        # Just in case the target (or one or more of its parents) is a package
        # (we don't know that), add a dependency on the relevant __init__ module.
        #
        # If there's no matching __init__ (either no __init__.py provided, or
        # the target is just a module), this is harmless - we just generate a
        # spurious dependency on a module that doesn't even exist.
        #
        # Since nonexistent modules are not in the analyzed set (i.e. do not
        # appear as keys of self.modules), prepare_graph will ignore them.
        #
        # TODO: This would be a problem for a simple plain-text output that doesn't use the graph.
        modpath = target_module.split(".")
        for k in range(1, len(modpath) + 1):
            base = ".".join(modpath[:k])
            possible_init = base + ".__init__"
            if possible_init != m:  # will happen when current_module is somepackage.__init__ itself
                self.modules[m].add(possible_init)
                self.logger.debug("    added possible implicit use of '{}'".format(possible_init))

    def visit_Import(self, node):
        self.logger.debug(
            "{}:{}: Import {}".format(self.current_module, node.lineno, [alias.name for alias in node.names])
        )
        for alias in node.names:
            self.add_dependency(alias.name)  # alias.asname not relevant for our purposes

    def visit_ImportFrom(self, node):
        # from foo import some_symbol
        if node.module:
            self.logger.debug(
                "{}:{}: ImportFrom '{}', relative import level {}".format(
                    self.current_module, node.lineno, node.module, node.level
                )
            )
            absname = resolve(self.current_module, node.module, node.level)
            if node.level > 0:
                self.logger.debug("    resolved relative import to '{}'".format(absname))
            self.add_dependency(absname)

        # from . import foo  -->  module = None; now the **names** refer to modules
        else:
            for alias in node.names:
                self.logger.debug(
                    "{}:{}: ImportFrom '{}', target module '{}', relative import level {}".format(
                        self.current_module, node.lineno, "." * node.level, alias.name, node.level
                    )
                )
                absname = resolve(self.current_module, alias.name, node.level)
                if node.level > 0:
                    self.logger.debug("    resolved relative import to '{}'".format(absname))
                self.add_dependency(absname)

    # --------------------------------------------------------------------------------

    def detect_cycles(self):
        """Postprocessing. Detect import cycles.

        Return format is `[(prefix, cycle), ...]` where `prefix` is the
        non-cyclic prefix of the import chain, and `cycle` contains only
        the cyclic part (where the first and last elements are the same).
        """
        cycles = []

        def walk(m, seen=None, trace=None):
            trace = (trace or []) + [m]
            seen = seen or set()
            if m in seen:
                cycles.append(trace)
                return
            seen = seen | {m}
            deps = self.modules[m]
            for d in sorted(deps):
                if d in self.modules:
                    walk(d, seen, trace)

        for root in sorted(self.modules):
            walk(root)

        # For each detected cycle, report the non-cyclic prefix and the cycle separately
        out = []
        for cycle in cycles:
            offender = cycle[-1]
            k = cycle.index(offender)
            out.append((cycle[:k], cycle[k:]))
        return out

    def prepare_graph(self):  # same format as in pyan.analyzer
        """Postprocessing. Prepare data for pyan.visgraph for graph file generation."""
        self.nodes = {}  # Node name: list of Node objects (in possibly different namespaces)
        self.uses_edges = {}
        # we have no defines_edges, which doesn't matter as long as we don't enable that option in visgraph.

        # TODO: Right now we care only about modules whose files we read.
        # TODO: If we want to include in the graph also targets that are not in the analyzed set,
        # TODO: then we could create nodes also for the modules listed in the *values* of self.modules.
        for m in self.modules:
            ns, mod = split_module_name(m)
            package = os.path.dirname(self.fullpaths[m])
            # print("{}: ns={}, mod={}, fn={}".format(m, ns, mod, fn))
            # HACK: The `filename` attribute of the node determines the visual color.
            # HACK: We are visualizing at module level, so color by package.
            # TODO: If we are analyzing files from several projects in the same run,
            # TODO: it could be useful to decide the hue by the top-level directory name
            # TODO: (after the './' if any), and lightness by the depth in each tree.
            # TODO: This would be most similar to how Pyan does it for functions/classes.
            n = pyan.node.Node(namespace=ns, name=mod, ast_node=None, filename=package, flavor=pyan.node.Flavor.MODULE)
            n.defined = True
            # Pyan's analyzer.py allows several nodes to share the same short name,
            # which is used as the key to self.nodes; but we use the fully qualified
            # name as the key. Nevertheless, visgraph expects a format where the
            # values in the visitor's `nodes` attribute are lists.
            self.nodes[m] = [n]

        def add_uses_edge(from_node, to_node):
            if from_node not in self.uses_edges:
                self.uses_edges[from_node] = set()
            self.uses_edges[from_node].add(to_node)

        for m, deps in self.modules.items():
            for d in deps:
                n_from = self.nodes.get(m)
                n_to = self.nodes.get(d)
                if n_from and n_to:
                    add_uses_edge(n_from[0], n_to[0])

        # sanity check output
        for m, deps in self.uses_edges.items():
            assert m.get_name() in self.nodes
            for d in deps:
                assert d.get_name() in self.nodes


def main():
    usage = """usage: %prog FILENAME... [--dot|--tgf|--yed]"""
    desc = "Analyse one or more Python source files and generate an approximate module dependency graph."
    parser = OptionParser(usage=usage, description=desc)
    parser.add_option("--dot", action="store_true", default=False, help="output in GraphViz dot format")
    parser.add_option("--tgf", action="store_true", default=False, help="output in Trivial Graph Format")
    parser.add_option("--yed", action="store_true", default=False, help="output in yEd GraphML Format")
    parser.add_option("-f", "--file", dest="filename", help="write graph to FILE", metavar="FILE", default=None)
    parser.add_option("-l", "--log", dest="logname", help="write log to LOG", metavar="LOG")
    parser.add_option("-v", "--verbose", action="store_true", default=False, dest="verbose", help="verbose output")
    parser.add_option(
        "-V",
        "--very-verbose",
        action="store_true",
        default=False,
        dest="very_verbose",
        help="even more verbose output (mainly for debug)",
    )
    parser.add_option(
        "-c",
        "--colored",
        action="store_true",
        default=False,
        dest="colored",
        help="color nodes according to namespace [dot only]",
    )
    parser.add_option(
        "-g",
        "--grouped",
        action="store_true",
        default=False,
        dest="grouped",
        help="group nodes (create subgraphs) according to namespace [dot only]",
    )
    parser.add_option(
        "-e",
        "--nested-groups",
        action="store_true",
        default=False,
        dest="nested_groups",
        help="create nested groups (subgraphs) for nested namespaces (implies -g) [dot only]",
    )
    parser.add_option(
        "-C",
        "--cycles",
        action="store_true",
        default=False,
        dest="cycles",
        help="detect import cycles and print report to stdout",
    )
    parser.add_option(
        "--dot-rankdir",
        default="TB",
        dest="rankdir",
        help=(
            "specifies the dot graph 'rankdir' property for "
            "controlling the direction of the graph. "
            "Allowed values: ['TB', 'LR', 'BT', 'RL']. "
            "[dot only]"
        ),
    )
    parser.add_option(
        "-a", "--annotated", action="store_true", default=False, dest="annotated", help="annotate with module location"
    )

    options, args = parser.parse_args()
    filenames = [fn2 for fn in args for fn2 in glob(fn, recursive=True)]
    if len(args) == 0:
        parser.error("Need one or more filenames to process")

    if options.nested_groups:
        options.grouped = True

    graph_options = {
        "draw_defines": False,  # we have no defines edges
        "draw_uses": True,
        "colored": options.colored,
        "grouped_alt": False,
        "grouped": options.grouped,
        "nested_groups": options.nested_groups,
        "annotated": options.annotated,
    }

    # TODO: use an int argument for verbosity
    logger = logging.getLogger(__name__)
    if options.very_verbose:
        logger.setLevel(logging.DEBUG)
    elif options.verbose:
        logger.setLevel(logging.INFO)
    else:
        logger.setLevel(logging.WARN)
    logger.addHandler(logging.StreamHandler())
    if options.logname:
        handler = logging.FileHandler(options.logname)
        logger.addHandler(handler)

    # run the analysis
    v = ImportVisitor(filenames, logger)

    # Postprocessing: detect import cycles
    #
    # NOTE: Because this is a static analysis, it doesn't care about the order
    # the code runs in any particular invocation of the software. Every
    # analyzed module is considered as a possible entry point to the program,
    # and all cycles (considering *all* possible branches *at any step* of
    # *each* import chain) will be mapped recursively.
    #
    # Obviously, this easily leads to a combinatoric explosion. In a mid-size
    # project (~20k SLOC), the analysis may find thousands of unique import
    # cycles, most of which are harmless.
    #
    # Many cycles appear due to package A importing something from package B
    # (possibly from one of its submodules) and vice versa, when both packages
    # have an __init__ module. If they don't actually try to import any names
    # that only become defined after the init has finished running, it's
    # usually fine.
    #
    # (Init modules often import names from their submodules to the package's
    # top-level namespace; those names can be reliably accessed only after the
    # init module has finished running. But importing names directly from the
    # submodule where they are defined is fine also during the init.)
    #
    # But if your program is crashing due to a cyclic import, you already know
    # in any case *which* import cycle is causing it, just by looking at the
    # stack trace. So this analysis is just extra information that says what
    # other cycles exist, if any.
    if options.cycles:
        cycles = v.detect_cycles()
        if not cycles:
            print("No import cycles detected.")
        else:
            unique_cycles = set()
            for prefix, cycle in cycles:
                unique_cycles.add(tuple(cycle))
            print("Detected the following import cycles (n_results={}).".format(len(unique_cycles)))

            def stats():
                lengths = [len(x) - 1 for x in unique_cycles]  # number of modules in the cycle

                def mean(lst):
                    return sum(lst) / len(lst)

                def median(lst):
                    tmp = list(sorted(lst))
                    n = len(lst)
                    if n % 2 == 1:
                        return tmp[n // 2]  # e.g. tmp[5] if n = 11
                    else:
                        return (tmp[n // 2 - 1] + tmp[n // 2]) / 2  # e.g. avg of tmp[4] and tmp[5] if n = 10

                return min(lengths), mean(lengths), median(lengths), max(lengths)

            print(
                "Number of modules in a cycle: min = {}, average = {:0.2g}, median = {:0.2g}, max = {}".format(*stats())
            )
            for c in sorted(unique_cycles):
                print("    {}".format(c))

    # # we could generate a plaintext report like this (with caveats; see TODO above)
    # ms = v.modules
    # for m in sorted(ms):
    #     print(m)
    #     for d in sorted(ms[m]):
    #         print("    {}".format(d))

    # Postprocessing: format graph report
    make_graph = options.dot or options.tgf or options.yed
    if make_graph:
        v.prepare_graph()
        # print(v.nodes, v.uses_edges)
        graph = pyan.visgraph.VisualGraph.from_visitor(v, options=graph_options, logger=logger)

    if options.dot:
        writer = pyan.writers.DotWriter(
            graph, options=["rankdir=" + options.rankdir], output=options.filename, logger=logger
        )
    if options.tgf:
        writer = pyan.writers.TgfWriter(graph, output=options.filename, logger=logger)
    if options.yed:
        writer = pyan.writers.YedWriter(graph, output=options.filename, logger=logger)
    if make_graph:
        writer.run()


if __name__ == "__main__":
    main()
