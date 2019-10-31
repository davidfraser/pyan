# -*- coding: utf-8; -*-
"""A simple import analyzer. Visualize dependencies between modules."""

import ast
import os
import logging

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

blacklist = (".git", "build", "dist", "test")
def find_py_files(basedir):
    py_files = []
    for root, dirs, files in os.walk(basedir):
        for x in blacklist:  # don't visit blacklisted dirs
            if x in dirs:
                dirs.remove(x)
        for filename in files:
            if filename.endswith(".py"):
                fullpath = os.path.join(root, filename)
                py_files.append(fullpath)
    return py_files

def resolve(current_module, target_module, level):
    """Return fully qualified name of the target_module in an import.

    Resolves relative imports (level > 0) using current_module as the starting point.
    """
    if level < 0:
        raise ValueError("Relative import level must be >= 0, got {}".format(level))
    if level == 0:  # absolute import
        return target_module
    # level > 0 (let's have some simplistic support for relative imports)
    base = current_module
    for _ in range(level):
        k = base.rfind('.')
        if k == -1:
            raise ValueError("Relative import level {} too large for module name {}".format(level, current_module))
        base = base[:k]
    return '.'.join((base, target_module))

class ImportVisitor(ast.NodeVisitor):
    def __init__(self, basedir):
        self.modules = {}    # modname: {dep0, dep1, ...}
        self.filenames = {}  # modname: filename
        self.analyze(basedir)

    def analyze(self, basedir):
        for fullpath in find_py_files(basedir):
            with open(fullpath, "rt", encoding="utf-8") as f:
                content = f.read()
            m = filename_to_module_name(fullpath)
            self.current_module = m
            self.filenames[m] = fullpath
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
            package = os.path.dirname(self.filenames[m])
            # print("{}: ns={}, mod={}, fn={}".format(m, ns, mod, fn))
            n = pyan.node.Node(namespace=ns,
                               name=mod,
                               ast_node=None,
                               filename=package,  # HACK: visualizing at module level, so color by package.
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
    v = ImportVisitor(".")

    # plaintext report
    ms = v.modules
    for m in sorted(ms):
        print(m)
        for d in sorted(ms[m]):
            print("    {}".format(d))

    # dot report
    v.prepare_graph()
#    print(v.nodes, v.uses_edges)
    logger = logging.getLogger(__name__)

    graph_options = {"colored": True, "nested": False, "grouped_alt": False, "grouped": False,
                     "annotated": True, "draw_defines": False, "draw_uses": True}
    graph = pyan.visgraph.VisualGraph.from_visitor(v, options=graph_options, logger=logger)
    writer = pyan.writers.DotWriter(graph,
                                    options=['rankdir=TB'],
                                    output="modvis_output.dot",
                                    logger=logger)
    writer.run()

if __name__ == '__main__':
    main()
