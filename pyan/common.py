#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Parts shared between analyzer and graphgen.

Created on Mon Nov 13 03:33:00 2017

Original code by Edmund Horner.
Further development by Juha Jeronen.
"""

from sys import stderr
import os.path
import ast


class MsgLevel:
    ERROR   = 0
    WARNING = 1
    INFO    = 2
    DEBUG   = 3

class MsgPrinter:
    def __init__(self, verbosity=MsgLevel.WARNING):
        self.verbosity = verbosity

    def message(self, msg, level):
        if level <= self.verbosity:
            print(msg, file=stderr)

    def set_verbosity(self, verbosity):
        self.verbosity = verbosity


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

def format_alias(x):
    """Return human-readable description of an ast.alias (used in Import and ImportFrom nodes)."""
    if not isinstance(x, ast.alias):
        raise TypeError("Can only format an ast.alias; got %s" % type(x))

    if x.asname is not None:
        return "%s as %s" % (x.name, x.asname)
    else:
        return "%s" % (x.name)

def get_ast_node_name(x):
    """Return human-readable name of ast.Attribute or ast.Name. Pass through anything else."""
    if isinstance(x, ast.Attribute):
        # x.value might also be an ast.Attribute (think "x.y.z")
        return "%s.%s" % (get_ast_node_name(x.value), x.attr)
    elif isinstance(x, ast.Name):
        return x.id
    else:
        return x

# Helper for handling binding forms.
def sanitize_exprs(exprs):
    """Convert ast.Tuples in exprs to Python tuples; wrap result in a Python tuple."""
    def process(expr):
        if isinstance(expr, (ast.Tuple, ast.List)):
            return expr.elts  # .elts is a Python tuple
        else:
            return [expr]
    if isinstance(exprs, (tuple, list)):
        return [process(expr) for expr in exprs]
    else:
        return process(exprs)


class Node:
    """A node is an object in the call graph.  Nodes have names, and are in
    namespaces.  The full name of a node is its namespace, a dot, and its name.
    If the namespace is None, it is rendered as *, and considered as an unknown
    node.  The meaning of this is that a use-edge to an unknown node is created
    when the analysis cannot determine which actual node is being used."""

    def __init__(self, namespace, name, ast_node, filename):
        self.namespace = namespace
        self.name = name
        self.ast_node = ast_node
        self.filename = filename
        self.defined = namespace is None  # assume that unknown nodes are defined

    def get_short_name(self):
        """Return the short name (i.e. excluding the namespace), of this Node.
        Names of unknown nodes will include the *. prefix."""

        if self.namespace is None:
            return '*.' + self.name
        else:
            return self.name

    def get_annotated_name(self):
        """Return the short name, plus module and line number of definition site, if available.
        Names of unknown nodes will include the *. prefix."""
        if self.namespace is None:
            return '*.' + self.name
        else:
            if self.get_level() >= 1 and self.ast_node is not None:
                return "%s\n(%s:%d)" % (self.name, self.filename, self.ast_node.lineno)
            else:
                return self.name

    def get_long_annotated_name(self):
        """Return the short name, plus namespace, and module and line number of definition site, if available.
        Names of unknown nodes will include the *. prefix."""
        if self.namespace is None:
            return '*.' + self.name
        else:
            if self.get_level() >= 1:
                if self.ast_node is not None:
                    return "%s\\n\\n(%s:%d,\\nin %s)" % (self.name, self.filename, self.ast_node.lineno, self.namespace)
                else:
                    return "%s\\n\\n(in %s)" % (self.name, self.namespace)
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
        """Return the name of the top-level namespace of this node, or "" if none."""
        if self.namespace == "":
            return ""
        if self.namespace is None:  # group all unknowns in one namespace, "*"
            return "*"

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


class Scope:
    """Adaptor that makes scopes look somewhat like those from the Python 2
    compiler module, as far as Pyan's CallGraphVisitor is concerned."""

    def __init__(self, table):
        """table: SymTable instance from symtable.symtable()"""
        name = table.get_name()
        if name == 'top':
            name = ''  # Pyan defines the top level as anonymous
        self.name = name
        self.type = table.get_type()  # useful for __repr__()
        self.defs = {iden:None for iden in table.get_identifiers()}  # name:assigned_value

    def __repr__(self):
        return "<Scope: %s %s>" % (self.type, self.name)
