#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Format-agnostic representation of the output graph."""

import colorsys
import logging
import re


class Colorizer:
    """Output graph color manager.

    We set node color by filename.

    HSL: hue = top-level namespace, lightness = nesting level, saturation constant.

    The "" namespace (for *.py files) gets the first color. Since its
    level is 0, its lightness will be 1.0, i.e. pure white regardless
    of the hue.
    """

    def __init__(self, num_colors, colored=True, logger=None):
        self.logger = logger or logging.getLogger(__name__)
        self.colored = colored

        self._hues = [j / num_colors for j in range(num_colors)]
        self._idx_of = {}  # top-level namespace: hue index
        self._idx = 0

    def _next_idx(self):
        result = self._idx
        self._idx += 1
        if self._idx >= len(self._hues):
            self.logger.warn("WARNING: colors wrapped")
            self._idx = 0
        return result

    def _node_to_idx(self, node):
        ns = node.filename
        self.logger.info("Coloring %s from file '%s'" % (node.get_short_name(), ns))
        if ns not in self._idx_of:
            self._idx_of[ns] = self._next_idx()
        return self._idx_of[ns]

    def get(self, node):  # return (group number, hue index)
        idx = self._node_to_idx(node)
        return (idx, self._hues[idx])

    def make_colors(self, node):  # return (group number, fill color, text color)
        if self.colored:
            idx, H = self.get(node)
            L = max([1.0 - 0.1 * node.get_level(), 0.1])
            S = 1.0
            A = 0.7  # make nodes translucent (to handle possible overlaps)
            fill_RGBA = self.htmlize_rgb(*colorsys.hls_to_rgb(H, L, S), A=A)

            # black text on light nodes, white text on (very) dark nodes.
            text_RGB = "#000000" if L >= 0.5 else "#ffffff"
        else:
            idx, _ = self.get(node)
            fill_RGBA = self.htmlize_rgb(1.0, 1.0, 1.0, 0.7)
            text_RGB = "#000000"
        return idx, fill_RGBA, text_RGB

    @staticmethod
    def htmlize_rgb(R, G, B, A=None):
        if A is not None:
            R, G, B, A = [int(255.0 * x) for x in (R, G, B, A)]
            return "#%02x%02x%02x%02x" % (R, G, B, A)
        else:
            R, G, B = [int(255.0 * x) for x in (R, G, B)]
            return "#%02x%02x%02x" % (R, G, B)


class VisualNode(object):
    """
    A node in the output graph: colors, internal ID, human-readable label, ...
    """

    def __init__(self, id, label="", flavor="", fill_color="", text_color="", group=""):
        self.id = id  # graphing software friendly label (no special chars)
        self.label = label  # human-friendly label
        self.flavor = flavor
        self.fill_color = fill_color
        self.text_color = text_color
        self.group = group

    def __repr__(self):
        optionals = [repr(s) for s in [self.label, self.flavor, self.fill_color, self.text_color, self.group] if s]
        if optionals:
            return "VisualNode(" + repr(self.id) + ", " + ", ".join(optionals) + ")"
        else:
            return "VisualNode(" + repr(self.id) + ")"


class VisualEdge(object):
    """
    An edge in the output graph.

    flavor is meant to be 'uses' or 'defines'
    """

    def __init__(self, source, target, flavor, color):
        self.source = source
        self.target = target
        self.flavor = flavor
        self.color = color

    def __repr__(self):
        return "Edge(" + self.source.label + " " + self.flavor + " " + self.target.label + ")"


class VisualGraph(object):
    def __init__(self, id, label, nodes=None, edges=None, subgraphs=None, grouped=False):
        self.id = id
        self.label = label
        self.nodes = nodes or []
        self.edges = edges or []
        self.subgraphs = subgraphs or []
        self.grouped = grouped

    @classmethod
    def from_visitor(cls, visitor, options=None, logger=None):
        colored = options.get("colored", False)
        nested = options.get("nested_groups", False)
        grouped_alt = options.get("grouped_alt", False)
        grouped = nested or options.get("grouped", False)  # nested -> grouped
        annotated = options.get("annotated", False)
        draw_defines = options.get("draw_defines", False)
        draw_uses = options.get("draw_uses", False)

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
                def labeler(n):
                    return n.get_annotated_name()

            else:
                # the node label is the only place to put the namespace info
                def labeler(n):
                    return n.get_long_annotated_name()

        else:

            def labeler(n):
                return n.get_short_name()

        logger = logger or logging.getLogger(__name__)

        # collect and sort defined nodes
        visited_nodes = []
        for name in visitor.nodes:
            for node in visitor.nodes[name]:
                if node.defined:
                    visited_nodes.append(node)
        visited_nodes.sort(key=lambda x: (x.namespace, x.name))

        def find_filenames():
            filenames = set()
            for node in visited_nodes:
                filenames.add(node.filename)
            return filenames

        colorizer = Colorizer(num_colors=len(find_filenames()) + 1, colored=colored, logger=logger)

        nodes_dict = dict()
        root_graph = cls("G", label="", grouped=grouped)
        subgraph = root_graph
        namespace_stack = []
        prev_namespace = ""  # The namespace '' is first in visited_nodes.
        for node in visited_nodes:
            logger.info("Looking at %s" % node.name)

            # Create the node itself and add it to nodes_dict
            idx, fill_RGBA, text_RGB = colorizer.make_colors(node)
            visual_node = VisualNode(
                id=node.get_label(),
                label=labeler(node),
                flavor=repr(node.flavor),
                fill_color=fill_RGBA,
                text_color=text_RGB,
                group=idx,
            )
            nodes_dict[node] = visual_node

            # next namespace?
            if grouped and node.namespace != prev_namespace:
                if not prev_namespace:
                    logger.info("New namespace %s" % (node.namespace))
                else:
                    logger.info("New namespace %s, old was %s" % (node.namespace, prev_namespace))
                prev_namespace = node.namespace

                label = node.get_namespace_label()
                subgraph = cls(label, node.namespace)

                if nested:
                    # Pop the stack until the newly found namespace is within
                    # one of the parent namespaces, or until the stack runs out
                    # (i.e. this is a sibling).
                    if len(namespace_stack):
                        m = re.match(namespace_stack[-1].label, node.namespace)
                        # The '.' check catches siblings in cases like
                        # MeshGenerator vs. Mesh.
                        while m is None or m.end() == len(node.namespace) or node.namespace[m.end()] != ".":
                            namespace_stack.pop()
                            if not len(namespace_stack):
                                break
                            m = re.match(namespace_stack[-1].label, node.namespace)
                    parentgraph = namespace_stack[-1] if len(namespace_stack) else root_graph
                    parentgraph.subgraphs.append(subgraph)

                    namespace_stack.append(subgraph)
                else:
                    root_graph.subgraphs.append(subgraph)

            subgraph.nodes.append(visual_node)

        # Now add edges
        if draw_defines or grouped_alt:
            # If grouped, use gray lines so they won't visually obstruct
            # the "uses" lines.
            #
            # If not grouped, create lines for defines, but make them
            # fully transparent. This helps GraphViz's layout algorithms
            # place closer together those nodes that are linked by a
            # defines relationship.
            #
            color = "#838b8b" if draw_defines else "#ffffff00"
            for n in visitor.defines_edges:
                if n.defined:
                    for n2 in visitor.defines_edges[n]:
                        if n2.defined:
                            root_graph.edges.append(VisualEdge(nodes_dict[n], nodes_dict[n2], "defines", color))

        if draw_uses:
            color = "#000000"
            for n in visitor.uses_edges:
                if n.defined:
                    for n2 in visitor.uses_edges[n]:
                        if n2.defined:
                            root_graph.edges.append(VisualEdge(nodes_dict[n], nodes_dict[n2], "uses", color))

        return root_graph
