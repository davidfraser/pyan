#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Abstract node representing data gathered from the analysis."""

from enum import Enum


def make_safe_label(label):
    """Avoid name clashes with GraphViz reserved words such as 'graph'."""
    unsafe_words = ("digraph", "graph", "cluster", "subgraph", "node")
    out = label
    for word in unsafe_words:
        out = out.replace(word, "%sX" % word)
    return out.replace(".", "__").replace("*", "")


class Flavor(Enum):
    """Flavor describes the kind of object a node represents."""

    UNSPECIFIED = "---"  # as it says on the tin
    UNKNOWN = "???"  # not determined by analysis (wildcard)

    NAMESPACE = "namespace"  # node representing a namespace
    ATTRIBUTE = "attribute"  # attr of something, but not known if class or func.

    IMPORTEDITEM = "import"  # imported item of unanalyzed type

    MODULE = "module"
    CLASS = "class"
    FUNCTION = "function"
    METHOD = "method"  # instance method
    STATICMETHOD = "staticmethod"
    CLASSMETHOD = "classmethod"
    NAME = "name"  # Python name (e.g. "x" in "x = 42")

    # Flavors have a partial ordering in specificness of the information.
    #
    # This sort key scores higher on flavors that are more specific,
    # allowing selective overwriting (while defining the override rules
    # here, where that information belongs).
    #
    @staticmethod
    def specificity(flavor):
        if flavor in (Flavor.UNSPECIFIED, Flavor.UNKNOWN):
            return 0
        elif flavor in (Flavor.NAMESPACE, Flavor.ATTRIBUTE):
            return 1
        elif flavor == Flavor.IMPORTEDITEM:
            return 2
        else:
            return 3

    def __repr__(self):
        return self.value


class Node:
    """A node is an object in the call graph.

    Nodes have names, and reside in namespaces.

    The namespace is a dot-delimited string of names. It can be blank, '',
    denoting the top level.

    The fully qualified name of a node is its namespace, a dot, and its name;
    except at the top level, where the leading dot is omitted.

    If the namespace has the special value None, it is rendered as *, and the
    node is considered as an unknown node. A uses edge to an unknown node is
    created when the analysis cannot determine which actual node is being used.

    A graph node can be associated with an AST node from the analysis.
    This identifies the syntax object the node represents, and as a bonus,
    provides the line number at which the syntax object appears in the
    analyzed code. The filename, however, must be given manually.

    Nodes can also represent namespaces. These namespace nodes do not have an
    associated AST node. For a namespace node, the "namespace" argument is the
    **parent** namespace, and the "name" argument is the (last component of
    the) name of the namespace itself. For example,

        Node("mymodule", "main", None)

    represents the namespace "mymodule.main".

    Flavor describes the kind of object the node represents.
    See the Flavor enum for currently supported values.
    """

    def __init__(self, namespace, name, ast_node, filename, flavor):
        self.namespace = namespace
        self.name = name
        self.ast_node = ast_node
        self.filename = filename
        self.flavor = flavor
        self.defined = namespace is None  # assume that unknown nodes are defined

    def get_short_name(self):
        """Return the short name (i.e. excluding the namespace), of this Node.
        Names of unknown nodes will include the *. prefix."""

        if self.namespace is None:
            return "*." + self.name
        else:
            return self.name

    def get_annotated_name(self):
        """Return the short name, plus module and line number of definition site, if available.
        Names of unknown nodes will include the *. prefix."""
        if self.namespace is None:
            return "*." + self.name
        else:
            if self.get_level() >= 1 and self.ast_node is not None:
                return "%s\\n(%s:%d)" % (self.name, self.filename, self.ast_node.lineno)
            else:
                return self.name

    def get_long_annotated_name(self):
        """Return the short name, plus namespace, and module and line number of definition site, if available.
        Names of unknown nodes will include the *. prefix."""
        if self.namespace is None:
            return "*." + self.name
        else:
            if self.get_level() >= 1:
                if self.ast_node is not None:
                    return "%s\\n\\n(%s:%d,\\n%s in %s)" % (
                        self.name,
                        self.filename,
                        self.ast_node.lineno,
                        repr(self.flavor),
                        self.namespace,
                    )
                else:
                    return "%s\\n\\n(%s in %s)" % (self.name, repr(self.flavor), self.namespace)
            else:
                return self.name

    def get_name(self):
        """Return the full name of this node."""

        if self.namespace == "":
            return self.name
        elif self.namespace is None:
            return "*." + self.name
        else:
            return self.namespace + "." + self.name

    def get_level(self):
        """Return the level of this node (in terms of nested namespaces).

        The level is defined as the number of '.' in the namespace, plus one.
        Top level is level 0.

        """
        if self.namespace == "":
            return 0
        else:
            return 1 + self.namespace.count(".")

    def get_toplevel_namespace(self):
        """Return the name of the top-level namespace of this node, or "" if none."""
        if self.namespace == "":
            return ""
        if self.namespace is None:  # group all unknowns in one namespace, "*"
            return "*"

        idx = self.namespace.find(".")
        if idx > -1:
            return self.namespace[0:idx]
        else:
            return self.namespace

    def get_label(self):
        """Return a label for this node, suitable for use in graph formats.
        Unique nodes should have unique labels; and labels should not contain
        problematic characters like dots or asterisks."""

        return make_safe_label(self.get_name())

    def get_namespace_label(self):
        """Return a label for the namespace of this node, suitable for use
        in graph formats. Unique nodes should have unique labels; and labels
        should not contain problematic characters like dots or asterisks."""

        return make_safe_label(self.namespace)

    def __repr__(self):
        return "<Node %s:%s>" % (repr(self.flavor), self.get_name())
