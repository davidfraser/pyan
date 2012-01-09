import sys
import compiler
from glob import glob
from optparse import OptionParser


class CallGraphVisitor(object):

    def __init__(self):
        self.defines_edges = {}
        self.uses_edges = {}
        self.name_stack = []
        self.known_callables = set()
    
    def visitClass(self, node):
        self.add_defines_edge(self.name_stack[-1], node.name)
        self.known_callables.add(node.name)
        self.name_stack.append(node.name)
        for b in node.bases:
            self.visit(b)
        self.visit(node.code)
        self.name_stack.pop()
        
    def visitFunction(self, node):
        if node.name == '__init__':
            node_name = self.name_stack[-1]
        else:
            node_name = node.name
        self.add_defines_edge(self.name_stack[-1], node_name)
        self.known_callables.add(node_name)
        self.name_stack.append(node_name)
        for d in node.defaults:
            self.visit(d)
        self.visit(node.code)
        self.name_stack.pop()
        
    def visitImport(self, node):
        for import_item in node.names:
            tgt_name = import_item[0]
            self.add_uses_edge(self.name_stack[-1], tgt_name)
        
    def visitFrom(self, node):
        tgt_name = node.modname
        self.add_uses_edge(self.name_stack[-1], tgt_name)
        
    def visitName(self, node):
        tgt_name = node.name
        self.add_uses_edge(self.name_stack[-1], tgt_name)
    
    def visitGetattr(self, node):
        tgt_name = node.attrname
        self.add_uses_edge(self.name_stack[-1], tgt_name)
    
    def add_defines_edge(self, from_name, to_name):
        if from_name not in self.defines_edges:
            self.defines_edges[from_name] = set()
        self.defines_edges[from_name].add(to_name)
    
    def add_uses_edge(self, from_name, to_name):
        if from_name not in self.uses_edges:
            self.uses_edges[from_name] = set()
        self.uses_edges[from_name].add(to_name)
    
    def to_dot(self):
        s = """digraph G {\n"""
        for n in self.defines_edges:
            s += """    %s;\n""" % n
            for n2 in self.defines_edges[n]:
                if n2 in self.known_callables and n2 != n:
                    s += """    %s -> %s [style="dashed"];\n""" % (n, n2)
        for n in self.uses_edges:
            if n not in self.defines_edges:
                s += """    %s;\n""" % n
            for n2 in self.uses_edges[n]:
                if n2 in self.known_callables and n2 != n:
                    s += """    %s -> %s;\n""" % (n, n2)
        s += """}\n"""
        return s
    
    def to_tgf(self):
        s = ''
        i = 1
        id_map = {}
        for n in self.defines_edges:
            s += """%d %s\n""" % (i, n)
            id_map[n] = i
            i += 1
        for n in self.uses_edges:
            if n in id_map:
                continue
            s += """%d %s\n""" % (i, n)
            id_map[n] = i
            i += 1
        
        s += """#\n"""
        
        for n in self.defines_edges:
            for n2 in self.defines_edges[n]:
                if n2 in self.known_callables and n2 != n:
                    i1 = id_map[n]
                    i2 = id_map[n2]
                    s += """%d %d D\n""" % (i1, i2)
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                if n2 in self.known_callables and n2 != n:
                    i1 = id_map[n]
                    i2 = id_map[n2]
                    s += """%d %d U\n""" % (i1, i2)
        return s


def main():
    usage = """usage: %prog FILENAME... [--dot|--tgf]"""
    desc = """Analyse one or more Python source files and generate an approximate call graph of the modules, classes and functions within them."""
    parser = OptionParser(usage=usage, description=desc)
    parser.add_option("--dot",
                      action="store_true", default=False,
                      help="output in GraphViz dot format")
    parser.add_option("--tgf",
                      action="store_true", default=False,
                      help="output in Trivial Graph Format")

    options, args = parser.parse_args()
    filenames = [fn2 for fn in args for fn2 in glob(fn)]
    if len(args) == 0:
        parser.error('Need one or more filenames to process')
    
    v = CallGraphVisitor()
    for filename in filenames:
        ast = compiler.parseFile(filename)
        module_name = filename.replace('.py', '')
        v.name_stack = [module_name]
        v.known_callables.add(module_name)
        compiler.walk(ast, v)
    if options.dot:
        print v.to_dot()
    if options.tgf:
        print v.to_tgf()


if __name__ == '__main__':
    main()
