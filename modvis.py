#!/usr/bin/env python3
# -*- coding: utf-8; -*-
"""A simple import analyzer. Visualize dependencies between modules."""

import ast
import os
import logging
from glob import glob
from optparse import OptionParser  # TODO: migrate to argparse

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
        fullpath = fullpath[len(rel):]
    fullpath = fullpath[:-3]  # remove .py
    return fullpath.replace(os.path.sep, '.')

def split_module_name(m):
    """'fully.qualified.name' -> ('fully.qualified', 'name')"""
    k = m.rfind('.')
    if k == -1:
        return ("", m)
    return (m[:k], m[(k + 1):])

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

    Resolves relative imports (level > 0) using current_module as the starting point.
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
    return '.'.join((base, target_module))

class ImportVisitor(ast.NodeVisitor):
    def __init__(self, filenames):
        self.modules = {}    # modname: {dep0, dep1, ...}
        self.fullpaths = {}  # modname: fullpath
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

    def visit_Import(self, node):
        # print(self.current_module, "Import", [alias.name for alias in node.names])
        for alias in node.names:
            self.add_dependency(alias.name)  # alias.asname not relevant for our purposes

    def visit_ImportFrom(self, node):
        # print(self.current_module, "ImportFrom", node.module, node.level)
        self.add_dependency(resolve(self.current_module, node.module, node.level))

    # --------------------------------------------------------------------------------

    def prepare_graph(self):  # same format as in pyan.analyzer
        self.nodes = {}   # Node name: list of Node objects (in possibly different namespaces)
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
            n = pyan.node.Node(namespace=ns,
                               name=mod,
                               ast_node=None,
                               filename=package,
                               flavor=pyan.node.Flavor.MODULE)
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
    desc = ('Analyse one or more Python source files and generate an'
            'approximate module dependency graph.')
    parser = OptionParser(usage=usage, description=desc)
    parser.add_option("--dot",
                      action="store_true", default=False,
                      help="output in GraphViz dot format")
    parser.add_option("--tgf",
                      action="store_true", default=False,
                      help="output in Trivial Graph Format")
    parser.add_option("--yed",
                      action="store_true", default=False,
                      help="output in yEd GraphML Format")
    parser.add_option("-f", "--file", dest="filename",
                      help="write graph to FILE", metavar="FILE", default=None)
    parser.add_option("-l", "--log", dest="logname",
                      help="write log to LOG", metavar="LOG")
    parser.add_option("-v", "--verbose",
                      action="store_true", default=False, dest="verbose",
                      help="verbose output")
    parser.add_option("-V", "--very-verbose",
                      action="store_true", default=False, dest="very_verbose",
                      help="even more verbose output (mainly for debug)")
    parser.add_option("-c", "--colored",
                      action="store_true", default=False, dest="colored",
                      help="color nodes according to namespace [dot only]")
    parser.add_option("-g", "--grouped",
                      action="store_true", default=False, dest="grouped",
                      help="group nodes (create subgraphs) according to namespace [dot only]")
    parser.add_option("-e", "--nested-groups",
                      action="store_true", default=False, dest="nested_groups",
                      help="create nested groups (subgraphs) for nested namespaces (implies -g) [dot only]")
    parser.add_option("--dot-rankdir", default="TB", dest="rankdir",
                      help=(
                          "specifies the dot graph 'rankdir' property for "
                          "controlling the direction of the graph. "
                          "Allowed values: ['TB', 'LR', 'BT', 'RL']. "
                          "[dot only]"))
    parser.add_option("-a", "--annotated",
                      action="store_true", default=False, dest="annotated",
                      help="annotate with module location")

    options, args = parser.parse_args()
    filenames = [fn2 for fn in args for fn2 in glob(fn)]
    if len(args) == 0:
        parser.error('Need one or more filenames to process')

    if options.nested_groups:
        options.grouped = True

    graph_options = {
            'draw_defines': False,  # we have no defines edges
            'draw_uses': True,
            'colored': options.colored,
            'grouped_alt': False,
            'grouped': options.grouped,
            'nested_groups': options.nested_groups,
            'annotated': options.annotated}

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
    v = ImportVisitor(filenames)

    # # we could generate a plaintext report like this
    # ms = v.modules
    # for m in sorted(ms):
    #     print(m)
    #     for d in sorted(ms[m]):
    #         print("    {}".format(d))

    # format graph report
    v.prepare_graph()
#    print(v.nodes, v.uses_edges)
    logger = logging.getLogger(__name__)

    graph = pyan.visgraph.VisualGraph.from_visitor(v, options=graph_options, logger=logger)
    if options.dot:
        writer = pyan.writers.DotWriter(graph,
                                        options=['rankdir=' + options.rankdir],
                                        output=options.filename,
                                        logger=logger)
    if options.tgf:
        writer = pyan.writers.TgfWriter(
                graph, output=options.filename, logger=logger)
    if options.yed:
        writer = pyan.writers.YedWriter(
                graph, output=options.filename, logger=logger)
    writer.run()

if __name__ == '__main__':
    main()
