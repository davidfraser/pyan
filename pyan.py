#!/usr/bin/env python

"""
    pyan.py - Generate approximate call graphs for Python programs.

    This program takes one or more Python source files, does a superficial
    analysis, and constructs a directed graph of the objects in the combined
    source, and how they define or use each other.  The graph can be output
    for rendering by e.g. GraphViz or yEd.
"""

import compiler
from glob import glob
from optparse import OptionParser
import os.path
import logging

from graph import Graph
from writers import DotWriter, TgfWriter, YedWriter


class Node(object):
    """A node is an object in the call graph.  Nodes have names, and are in
    namespaces.  The full name of a node is its namespace, a dot, and its name.
    If the namespace is Null, it is rendered as *, and considered as an unknown
    node.  The meaning of this is that a use-edge to an unknown node is created
    when the analysis cannot determine which actual node is being used."""

    def __init__(self, namespace, name, orig_node):
        self.namespace = namespace
        self.name = name
        self.defined = namespace is None
        self.orig_node = orig_node

    def __cmp__(self, other):
        if self.get_name() > other.get_name():
            return +1
        elif self.get_name() < other.get_name():
            return -1
        else:
            return 0

    def get_short_name(self):
        """Return the short name (i.e. excluding the namespace), of this Node.
        Names of unknown nodes will include the *. prefix."""

        if self.namespace is None:
            return '*.' + self.name
        else:
            return self.name

    def get_name(self):
        """Return the full name of this node."""

        if self.namespace == '':
            return self.name
        elif self.namespace is None:
            return '*.' + self.name
        else:
            return self.namespace + '.' + self.name

    def get_level(self):
        """Return the level of this node (in terms of nested namespaces).

        The level is defined as the number of '.' in the namespace, plus one.
        Top level is level 0.

        """
        if self.namespace == "":
            return 0
        else:
            return 1 + self.namespace.count('.')

    def get_toplevel_namespace(self):
        """Return the name of the top-level namespace of this node, or "" if
        none."""
        if self.namespace == "":
            return ""

        idx = self.namespace.find('.')
        if idx > -1:
            return self.namespace[0:idx]
        else:
            return self.namespace

    def get_label(self):
        """Return a label for this node, suitable for use in graph formats.
        Unique nodes should have unique labels; and labels should not contain
        problematic characters like dots or asterisks."""

        return self.get_name().replace('.', '__').replace('*', '')

    def __repr__(self):
        return '<Node %s>' % self.get_name()


class CallGraphVisitor(object):
    """A visitor that can be walked over a Python AST, and will derive
    information about the objects in the AST and how they use each other.

    A single CallGraphVisitor object can be run over several ASTs (from a
    set of source files).  The resulting information is the aggregate from
    all files.  This way use information between objects in different files
    can be gathered."""

    def __init__(self, logger=None):
        self.nodes = {}
        self.defines_edges = {}
        self.uses_edges = {}
        self.name_stack = []
        self.scope_stack = []
        self.last_value = None
        self.current_class = None
        self.logger = logger or logging.getLogger(__name__)

    @classmethod
    def from_filenames(cls, filenames, logger=None):
        v = cls(logger)
        v.module_names = {}

        # First find module full names for all files
        for filename in filenames:
            mod_name = get_module_name(filename)
            short_name = mod_name.rsplit('.', 1)[-1]
            v.module_names[short_name] = mod_name

        # Process the set of files, TWICE: so that forward references are
        # picked up
        for filename in filenames + filenames:
            ast = compiler.parseFile(filename)
            module_name = get_module_name(filename)
            v.module_name = module_name
            s = compiler.symbols.SymbolVisitor()
            compiler.walk(ast, s)
            v.scopes = s.scopes
            compiler.walk(ast, v)

        v.contract_nonexistents()
        v.expand_unknowns()
        v.cull_inherited()

        return v

    def visitModule(self, node):
        self.name_stack.append(self.module_name)
        self.scope_stack.append(self.scopes[node])
        self.visit(node.node)
        self.scope_stack.pop()
        self.name_stack.pop()
        self.last_value = None

    def visitClass(self, node):
        from_node = self.get_current_namespace()
        to_node = self.get_node(from_node.get_name(), node.name, node)
        if self.add_defines_edge(from_node, to_node):
            self.logger.info("Def from %s to Class %s" % (from_node, to_node))

        self.current_class = to_node

        self.name_stack.append(node.name)
        self.scope_stack.append(self.scopes[node])
        for b in node.bases:
            self.visit(b)
        self.visit(node.code)
        self.scope_stack.pop()
        self.name_stack.pop()

    def visitFunction(self, node):
        if node.name == '__init__':
            for d in node.defaults:
                self.visit(d)
            self.visit(node.code)
            return

        from_node = self.get_current_namespace()
        to_node = self.get_node(from_node.get_name(), node.name, node)
        if self.add_defines_edge(from_node, to_node):
            self.logger.info(
                    "Def from %s to Function %s" % (from_node, to_node))

        self.name_stack.append(node.name)
        self.scope_stack.append(self.scopes[node])
        for d in node.defaults:
            self.visit(d)
        self.visit(node.code)
        self.scope_stack.pop()
        self.name_stack.pop()

    def visitImport(self, node):
        for import_item in node.names:
            tgt_name = import_item[0].split('.', 1)[0]
            from_node = self.get_current_namespace()
            to_node = self.get_node('', tgt_name, node)
            if self.add_uses_edge(from_node, to_node):
                self.logger.info(
                        "Use from %s to Import %s"
                        % (from_node, to_node))

            if tgt_name in self.module_names:
                mod_name = self.module_names[tgt_name]
            else:
                mod_name = tgt_name
            tgt_module = self.get_node('', mod_name, node)
            self.set_value(tgt_name, tgt_module)

    def visitFrom(self, node):
        tgt_name = node.modname
        from_node = self.get_current_namespace()
        to_node = self.get_node(None, tgt_name, node)
        if self.add_uses_edge(from_node, to_node):
            self.logger.info("Use from %s to From %s" % (from_node, to_node))

        if tgt_name in self.module_names:
            mod_name = self.module_names[tgt_name]
        else:
            mod_name = tgt_name
        for name, new_name in node.names:
            if new_name is None:
                new_name = name
            tgt_module = self.get_node(mod_name, name, node)
            self.set_value(new_name, tgt_module)
            self.logger.info(
                    "From setting name %s to %s"
                    % (new_name, tgt_module))

    def visitConst(self, node):
        t = type(node.value)
        tn = t.__name__
        self.last_value = self.get_node('', tn, node)

    def visitAssAttr(self, node):
        save_last_value = self.last_value
        self.visit(node.expr)

        if (isinstance(self.last_value, Node) and
                self.last_value.orig_node in self.scopes):
            sc = self.scopes[self.last_value.orig_node]
            sc.defs[node.attrname] = save_last_value
            self.logger.info(
                    'assattr %s on %s to %s'
                    % (node.attrname, self.last_value, save_last_value))

        self.last_value = save_last_value

    def visitAssName(self, node):
        tgt_name = node.name
        self.set_value(tgt_name, self.last_value)

    def visitAssign(self, node):
        self.visit(node.expr)

        for ass in node.nodes:
            self.visit(ass)

        self.last_value = None

    def visitCallFunc(self, node):
        self.visit(node.node)

        for arg in node.args:
            self.visit(arg)

        if node.star_args is not None:
            self.visit(node.star_args)
        if node.dstar_args is not None:
            self.visit(node.dstar_args)

    def visitDiscard(self, node):
        self.visit(node.expr)
        self.last_value = None

    def visitName(self, node):
        if node.name == 'self' and self.current_class is not None:
            self.logger.info(
                    'name %s is maps to %s'
                    % (node.name, self.current_class))
            self.last_value = self.current_class
            return

        tgt_name = node.name
        from_node = self.get_current_namespace()
        to_node = self.get_value(tgt_name)
        # TODO if the name is a local variable (i.e. in the top scope), and
        # has no known value, then don't try to create a node for it.
        if not isinstance(to_node, Node):
            to_node = self.get_node(None, tgt_name, node)
        if self.add_uses_edge(from_node, to_node):
            self.logger.info("Use from %s to Name %s" % (from_node, to_node))

        self.last_value = to_node

    def visitGetattr(self, node):
        self.visit(node.expr)

        if (isinstance(self.last_value, Node) and
                self.last_value.orig_node in self.scopes and
                node.attrname in self.scopes[self.last_value.orig_node].defs):
            orig_node = self.scopes[self.last_value.orig_node]
            new_value = orig_node.defs[node.attrname]
            self.logger.info(
                    'getattr %s from %s returns %s'
                    % (node.attrname, self.last_value, new_value))
            self.last_value = new_value
            return

        tgt_name = node.attrname
        from_node = self.get_current_namespace()
        if (isinstance(self.last_value, Node) and
                self.last_value.namespace is not None):
            to_node = self.get_node(self.last_value.get_name(), tgt_name, node)
        else:
            to_node = self.get_node(None, tgt_name, node)
        if self.add_uses_edge(from_node, to_node):
            self.logger.info(
                    "Use from %s to Getattr %s" % (from_node, to_node))

        self.last_value = to_node

    def get_node(self, namespace, name, orig_node=None):
        """Return the unique node matching the namespace and name.
        Creates a new node if one doesn't already exist."""

        if name in self.nodes:
            for n in self.nodes[name]:
                if n.namespace == namespace:
                    return n

        n = Node(namespace, name, orig_node)

        if name in self.nodes:
            self.nodes[name].append(n)
        else:
            self.nodes[name] = [n]

        return n

    def get_current_namespace(self):
        """Return a node representing the current namespace."""

        namespace = '.'.join(self.name_stack[0:-1])
        name = self.name_stack[-1]
        return self.get_node(namespace, name, None)

    def find_scope(self, name):
        """Search in the scope stack for the top-most scope containing name."""

        for sc in reversed(self.scope_stack):
            if name in sc.defs:
                return sc
        return None

    def get_value(self, name):
        """Get the value of name in the current scope."""

        sc = self.find_scope(name)
        if sc is None:
            return None
        value = sc.defs[name]
        if isinstance(value, Node):
            return value
        return None

    def set_value(self, name, value):
        """Set the value of name in the current scope."""

        sc = self.find_scope(name)
        if sc is not None and isinstance(value, Node):
            sc.defs[name] = value
            self.logger.info('Set %s to %s' % (name, value))

    def add_defines_edge(self, from_node, to_node):
        """Add a defines edge in the graph between two nodes.
        N.B. This will mark both nodes as defined."""

        if from_node not in self.defines_edges:
            self.defines_edges[from_node] = set()
        if to_node in self.defines_edges[from_node]:
            return False
        self.defines_edges[from_node].add(to_node)
        from_node.defined = True
        to_node.defined = True
        return True

    def add_uses_edge(self, from_node, to_node):
        """Add a uses edge in the graph between two nodes."""

        if from_node not in self.uses_edges:
            self.uses_edges[from_node] = set()
        if to_node in self.uses_edges[from_node]:
            return False
        self.uses_edges[from_node].add(to_node)
        return True

    def contract_nonexistents(self):
        """For all use edges to non-existent (i.e. not defined nodes) X.name,
        replace with edge to *.name."""

        new_uses_edges = []
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                if n2.namespace is not None and not n2.defined:
                    n3 = self.get_node(None, n2.name, n2.orig_node)
                    new_uses_edges.append((n, n3))
                    self.logger.info(
                            "Contracting non-existent from %s to %s"
                            % (n, n2))

        for from_node, to_node in new_uses_edges:
            self.add_uses_edge(from_node, to_node)

    def expand_unknowns(self):
        """For each unknown node *.name, replace all its incoming edges with
        edges to X.name for all possible Xs."""

        new_defines_edges = []
        for n in self.defines_edges:
            for n2 in self.defines_edges[n]:
                if n2.namespace is None:
                    for n3 in self.nodes[n2.name]:
                        new_defines_edges.append((n, n3))

        for from_node, to_node in new_defines_edges:
            self.add_defines_edge(from_node, to_node)

        new_uses_edges = []
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                if n2.namespace is None:
                    for n3 in self.nodes[n2.name]:
                        new_uses_edges.append((n, n3))

        for from_node, to_node in new_uses_edges:
            self.add_uses_edge(from_node, to_node)

        for name in self.nodes:
            for n in self.nodes[name]:
                if n.namespace is None:
                    n.defined = False

    def cull_inherited(self):
        """For each use edge from W to X.name, if it also has an edge to W to
        Y.name where Y is used by X, then remove the first edge."""

        removed_uses_edges = []
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                inherited = False
                for n3 in self.uses_edges[n]:
                    if (n3.name == n2.name and
                            n2.namespace is not None and
                            n3.namespace is not None and
                            n3.namespace != n2.namespace):
                        if '.' in n2.namespace:
                            nsp2, p2 = n2.namespace.rsplit('.', 1)
                        else:
                            nsp2, p2 = '', n2.namespace
                        if '.' in n3.namespace:
                            nsp3, p3 = n3.namespace.rsplit('.', 1)
                        else:
                            nsp3, p3 = '', n3.namespace
                        pn2 = self.get_node(nsp2, p2, None)
                        pn3 = self.get_node(nsp3, p3, None)
                        if (pn2 in self.uses_edges and
                                pn3 in self.uses_edges[pn2]):
                            inherited = True

                if inherited and n in self.uses_edges:
                    removed_uses_edges.append((n, n2))
                    self.logger.info(
                            "Removing inherited edge from %s to %s"
                            % (n, n2))

        for from_node, to_node in removed_uses_edges:
            self.uses_edges[from_node].remove(to_node)


def get_module_name(filename):
    """Try to determine the full module name of a source file, by figuring out
    if its directory looks like a package (i.e. has an __init__.py file)."""

    if os.path.basename(filename) == '__init__.py':
        return get_module_name(os.path.dirname(filename))

    init_path = os.path.join(os.path.dirname(filename), '__init__.py')
    mod_name = os.path.basename(filename).replace('.py', '')

    if not os.path.exists(init_path):
        return mod_name

    return get_module_name(os.path.dirname(filename)) + '.' + mod_name


def main():
    usage = """usage: %prog FILENAME... [--dot|--tgf|--yed]"""
    desc = ('Analyse one or more Python source files and generate an'
            'approximate call graph of the modules, classes and functions'
            ' within them.')
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
    parser.add_option("-d", "--defines",
                      action="store_true", default=True, dest="draw_defines",
                      help="add edges for 'defines' relationships [default]")
    parser.add_option("-n", "--no-defines",
                      action="store_false", default=True, dest="draw_defines",
                      help="do not add edges for 'defines' relationships")
    parser.add_option("-u", "--uses",
                      action="store_true", default=True, dest="draw_uses",
                      help="add edges for 'uses' relationships [default]")
    parser.add_option("-N", "--no-uses",
                      action="store_false", default=True, dest="draw_uses",
                      help="do not add edges for 'uses' relationships")
    parser.add_option("-c", "--colored",
                      action="store_true", default=False, dest="colored",
                      help="color nodes according to namespace [dot only]")
    parser.add_option("-g", "--grouped",
                      action="store_true", default=False, dest="grouped",
                      help=(
                          "group nodes (create subgraphs) "
                          "according to namespace [dot only]"))
    parser.add_option("-e", "--nested-groups",
                      action="store_true", default=False, dest="nested_groups",
                      help=(
                          "create nested groups (subgraphs) "
                          "for nested namespaces (implies -g) [dot only]"))

    options, args = parser.parse_args()
    filenames = [fn2 for fn in args for fn2 in glob(fn)]
    if len(args) == 0:
        parser.error('Need one or more filenames to process')

    if options.nested_groups:
        options.grouped = True

    graph_options = {
            'draw_defines': options.draw_defines,
            'draw_uses': options.draw_uses,
            'colored': options.colored,
            'grouped': options.grouped,
            'nested_groups': options.nested_groups}

    if options.logname or options.verbose:
        logger = logging.getLogger(__name__)
        logger.setLevel(logging.INFO)
        if options.logname:
            handler = logging.FileHandler(options.logname)
            logger.addHandler(handler)
        if options.verbose:
            logger.addHandler(logging.StreamHandler())
    else:
        logger = None

    v = CallGraphVisitor.from_filenames(filenames, logger)

    graph = Graph.from_visitor(v, options=graph_options, logger=logger)

    if options.dot:
        writer = DotWriter(
                graph, output=options.filename, logger=logger)
        writer.run()

    if options.tgf:
        writer = TgfWriter(
                graph, output=options.filename, logger=logger)
        writer.run()

    if options.yed:
        writer = YedWriter(
                graph, output=options.filename, logger=logger)
        writer.run()

if __name__ == '__main__':
    main()
