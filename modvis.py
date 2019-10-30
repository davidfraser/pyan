# -*- coding: utf-8; -*-
"""A simple import analyzer. Visualize dependencies between modules."""

import ast
import os

import pyan.node
# from pyan.anutils import get_module_name

def get_module_name(fullpath):  # we need to see __init__, hence we don't use anutils.
    if not fullpath.endswith(".py"):
        raise ValueError("Expected a .py filename, got '{}'".format(fullpath))
    rel = ".{}".format(os.path.sep)  # ./
    if fullpath.startswith(rel):
        fullpath = fullpath[len(rel):]
    fullpath = fullpath[:-3]  # remove .py
    return fullpath.replace(os.path.sep, '.')

blacklist = (".git", "build", "dist")
def get_pyfiles(basedir):
    pyfiles = []
    for root, dirs, files in os.walk(basedir):
        for x in blacklist:  # don't visit blacklisted dirs
            if x in dirs:
                dirs.remove(x)
        for filename in files:
            if filename.endswith(".py"):
                fullpath = os.path.join(root, filename)
                pyfiles.append(fullpath)
    return pyfiles

def resolve(current_module, target_module, level):
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
        self.modules = {}  # modname: {used0, used1, ...}
        self.analyze(basedir)

    def analyze(self, basedir):
        for fullpath in get_pyfiles(basedir):
            with open(fullpath, "rt", encoding="utf-8") as f:
                content = f.read()
            self.current_module = get_module_name(fullpath)
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
            n = pyan.node.Node(namespace="",  # not used
                               name=m,
                               ast_node=None,
                               filename="",  # not used
                               flavor=pyan.node.Flavor.MODULE)
            n.defined = True
            self.nodes[m] = n

        def add_uses_edge(from_node, to_node):
            if to_node not in self.modules:
                return
            if from_node not in self.uses_edges:
                self.uses_edges[from_node] = set()
            self.uses_edges[from_node].add(to_node)

        for m, deps in self.modules.items():
            for d in deps:
                add_uses_edge(m, d)

        # sanity check output
        for m, deps in self.uses_edges.items():
            assert m in self.nodes
            for d in deps:
                assert d in self.nodes

def main():
    v = ImportVisitor(".")
    ms = v.modules
    for m in sorted(ms):
        print(m)
        for d in sorted(ms[m]):
            print("    {}".format(d))
    # v.prepare_graph()
    # print(v.nodes, v.uses_edges)

if __name__ == '__main__':
    main()
