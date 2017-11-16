#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Graph markup generator.

Created on Mon Nov 13 03:32:12 2017

Original code by Edmund Horner.
Coloring logic and grouping for GraphViz output by Juha Jeronen.
"""

# TODO: namespace mode, just one node per namespace and relations between them
#  - useful if the full output is too detailed to be visually readable
#  - scan the nodes and edges, basically generate a new graph and to_dot() that

import re
import colorsys

from .common import MsgPrinter, MsgLevel

def htmlize_rgb(R,G,B,A=None):
    if A is not None:
        R,G,B,A = [int(255.0*x) for x in (R,G,B,A)]
        return "#%02X%02X%02X%02X" % (R,G,B,A)
    else:
        R,G,B = [int(255.0*x) for x in (R,G,B)]
        return "#%02X%02X%02X" % (R,G,B)

# Set node color by top-level namespace.
#
# HSL: hue = top-level namespace, lightness = nesting level, saturation constant.
#
# The "" namespace (for *.py files) gets the first color. Since its
# level is 0, its lightness will be 1.0, i.e. pure white regardless
# of the hue.
#
class Colorizer:
    def __init__(self, n, msgprinter=None):  # n: number of hues
        if msgprinter is None:
            msgprinter = MsgPrinter()
        self.msgprinter = msgprinter

        self._hues = [j/n for j in range(n)]
        self._idx_of = {}  # top-level namespace: hue index
        self._idx = 0

    def _next_idx(self):
        result = self._idx
        self._idx += 1
        if self._idx >= len(self._hues):
            self.msgprinter.message("WARNING: colors wrapped", level=MsgLevel.WARNING)
            self._idx = 0
        return result

    def _node_to_idx(self, node):
        ns = node.filename
        self.msgprinter.message("Coloring %s from file '%s'" % (node.get_short_name(), ns), level=MsgLevel.INFO)
        if ns not in self._idx_of:
            self._idx_of[ns] = self._next_idx()
        return self._idx_of[ns]

    def get(self, node):  # return (group number, hue)
        idx = self._node_to_idx(node)
        return (idx,self._hues[idx])


class GraphGenerator:
    def __init__(self, analyzer, msgprinter=None):
        """analyzer: CallGraphVisitor instance"""
        self.analyzer = analyzer
        if msgprinter is None:
            msgprinter = MsgPrinter()
        self.msgprinter = msgprinter

    # GraphViz docs:
    # http://www.graphviz.org/doc/info/lang.html
    # http://www.graphviz.org/doc/info/attrs.html
    #
    def to_dot(self, draw_defines, draw_uses, colored, grouped, nested_groups, annotated, rankdir):
        """Return, as a string, a GraphViz .dot representation of the graph."""
        analyzer = self.analyzer

        # Terminology:
        #  - what Node calls "label" is a computer-friendly unique identifier
        #    for use in graphing tools
        #  - the "label" property of a GraphViz node is a **human-readable** name
        #
        # The annotation determines the human-readable name.
        #
        if annotated:
            if grouped:
                # group label includes namespace already
                label_node = lambda n: n.get_annotated_name()
            else:
                # the node label is the only place to put the namespace info
                label_node = lambda n: n.get_long_annotated_name()
        else:
            label_node = lambda n: n.get_short_name()

        # find out which nodes are defined (can be visualized)
        vis_node_list = []
        for name in analyzer.nodes:
            for n in analyzer.nodes[name]:
                if n.defined:
                    vis_node_list.append(n)
        # Sort by namespace for easy cluster generation.
        # Secondary sort by name to make the output have a deterministic ordering.
        vis_node_list.sort(key=lambda x: (x.namespace, x.name))

        def find_filenames():
            filenames = set()
            for node in vis_node_list:
                filenames.add(node.filename)
            return filenames
        colorizer = Colorizer(n=len(find_filenames())+1)

        s = """digraph G {\n"""

        graph_opts = {'rankdir': rankdir}

        # http://www.graphviz.org/doc/info/attrs.html#a:clusterrank
        if grouped:
            graph_opts['clusterrank'] = 'local'

        graph_opts = ', '.join(
            [key + '="' + value + '"' for key, value in graph_opts.items()]
        )
        s += """    graph [""" + graph_opts + """];\n"""

        # Write nodes and subgraphs
        #
        prev_namespace = ""  # The namespace "" (for .py files) is first in vis_node_list.
        namespace_stack = []
        indent = ""
        def update_indent():
            return " " * (4*len(namespace_stack))  # 4 spaces per level
        for n in vis_node_list:
            if grouped and n.namespace != prev_namespace:
                if nested_groups:
                    # Pop the stack until the newly found namespace is within one of the
                    # parent namespaces (i.e. this is a sibling at that level), or until
                    # the stack runs out.
                    #
                    if len(namespace_stack):
                        m = re.match(namespace_stack[-1], n.namespace)
                        # The '.' check catches siblings in cases like MeshGenerator vs. Mesh.
                        while m is None or n.namespace[m.end()] != '.':
                            s += """%s}\n""" % indent  # terminate previous subgraph
                            namespace_stack.pop()
                            indent = update_indent()
                            if not len(namespace_stack):
                                break
                            m = re.match(namespace_stack[-1], n.namespace)
                    namespace_stack.append(n.namespace)
                    indent = update_indent()
                else:
                    if prev_namespace != "":
                        s += """%s}\n""" % indent  # terminate previous subgraph
                    else:
                        indent = " " * 4  # first subgraph begins, start indenting
                prev_namespace = n.namespace
                # Begin new subgraph for this namespace
                #
                # Name must begin with "cluster" to be recognized as a cluster by GraphViz.
                s += """%ssubgraph cluster_%s {\n""" % (indent, n.get_namespace_label())

                # translucent gray (no hue to avoid visual confusion with any group of colored nodes)
                s += """%s    graph [style="filled,rounded", fillcolor="#80808018", label="%s"];\n""" % (indent, n.namespace)

            # add the node itself
            if colored:
                idx,H = colorizer.get(n)
                L = max( [1.0 - 0.1*n.get_level(), 0.1] )
                S = 1.0
                A = 0.7  # make nodes translucent (to handle possible overlaps)
                fill_RGBA = htmlize_rgb(*colorsys.hls_to_rgb(H,L,S), A=A)

                # black text on light nodes, white text on (very) dark nodes.
                text_RGB = "#000000" if L >= 0.5 else "#FFFFFF"

                s += """%s    %s [label="%s", style="filled", fillcolor="%s", fontcolor="%s", group="%s"];\n""" % (indent, n.get_label(), label_node(n), fill_RGBA, text_RGB, idx)
            else:
                fill_RGBA = htmlize_rgb(1.0, 1.0, 1.0, 0.7)
                idx,_ = colorizer.get(n)
                s += """%s    %s [label="%s", style="filled", fillcolor="%s", fontcolor="#000000", group="%s"];\n""" % (indent, n.get_label(), label_node(n), fill_RGBA, idx)

        if grouped:
            if nested_groups:
                while len(namespace_stack):
                    s += """%s}\n""" % indent  # terminate all remaining subgraphs
                    namespace_stack.pop()
                    indent = update_indent()
            else:
                s += """%s}\n""" % indent  # terminate last subgraph

        # Write defines relationships
        #
        if draw_defines or not grouped:
            # If grouped, use gray lines so they won't visually obstruct the "uses" lines.
            #
            # If not grouped, create lines for defines, but make them fully transparent.
            # This helps GraphViz's layout algorithms place closer together those nodes
            # that are linked by a defines relationship.
            #
            color = "azure4" if draw_defines else "#FFFFFF00"
            for n in analyzer.defines_edges:
                if n.defined:
                    for n2 in analyzer.defines_edges[n]:
                        if n2.defined and n2 != n:
                            s += """    %s -> %s [style="dashed", color="%s"];\n""" % (n.get_label(), n2.get_label(), color)

        # Write uses relationships
        #
        if draw_uses:
            for n in analyzer.uses_edges:
                if n.defined:
                    for n2 in analyzer.uses_edges[n]:
                        if n2.defined:
                            s += """    %s -> %s;\n""" % (n.get_label(), n2.get_label())

        s += """}\n"""  # terminate "digraph G {"
        return s


    def to_tgf(self, draw_defines, draw_uses):
        """Return, as a string, a Trivial Graph Format representation of the graph. Advanced features not available."""

        analyzer = self.analyzer

        s = ''
        i = 1
        id_map = {}
        for name in analyzer.nodes:
            for n in analyzer.nodes[name]:
                if n.defined:
                    s += """%d %s\n""" % (i, n.get_short_name())
                    id_map[n] = i
                    i += 1
                #else:
                #    print("ignoring %s" % n, file=sys.stderr)

        s += """#\n"""

        if draw_defines:
            for n in analyzer.defines_edges:
                if n.defined:
                    for n2 in analyzer.defines_edges[n]:
                        if n2.defined and n2 != n:
                            i1 = id_map[n]
                            i2 = id_map[n2]
                            s += """%d %d D\n""" % (i1, i2)

        if draw_uses:
            for n in analyzer.uses_edges:
                if n.defined:
                    for n2 in analyzer.uses_edges[n]:
                        if n2.defined:
                            i1 = id_map[n]
                            i2 = id_map[n2]
                            s += """%d %d U\n""" % (i1, i2)
        return s
