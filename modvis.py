# -*- coding: utf-8; -*-
"""A simple import analyzer. Visualize dependencies between modules."""

import ast
import os

# from pyan.anutils import get_module_name

def get_module_name(fullpath):  # we need to see __init__, hence we don't use anutils.
    if not fullpath.endswith(".py"):
        raise ValueError("Expected a .py filename, got '{}'".format(fullpath))
    rel = ".{}".format(os.path.sep)  # ./
    if fullpath.startswith(rel):
        fullpath = fullpath[len(rel):]
    fullpath = fullpath[:-3]  # remove .py
    return fullpath.replace(os.path.sep, '.')

def get_pyfiles(basedir):
    pyfiles = []
    for root, dirs_, files in os.walk(basedir):
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

def main():
    v = ImportVisitor(".")
    ms = v.modules
    for m in sorted(ms):
        print(m)
        for d in sorted(ms[m]):
            print("    {}".format(d))

if __name__ == '__main__':
    main()
