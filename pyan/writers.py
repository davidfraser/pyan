#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Graph markup writers."""

import sys
import logging


class Writer(object):
    def __init__(self, graph, output=None, logger=None, tabstop=4):
        self.graph = graph
        self.output = output
        self.logger = logger or logging.getLogger(__name__)
        self.indent_level = 0
        self.tabstop = tabstop*' '

    def log(self, msg):
        self.logger.info(msg)

    def indent(self, level=1):
        self.indent_level += level

    def dedent(self, level=1):
        self.indent_level -= level

    def write(self, line):
        self.outstream.write(self.tabstop*self.indent_level+line+'\n')

    def run(self):
        self.log('%s running' % type(self))
        try:
            self.outstream = open(self.output, 'w')
        except TypeError:
            self.outstream = sys.stdout
        self.start_graph()
        self.write_subgraph(self.graph)
        self.write_edges()
        self.finish_graph()
        if self.output:
            self.outstream.close()

    def write_subgraph(self, graph):
        self.start_subgraph(graph)
        for node in graph.nodes:
            self.write_node(node)
        for subgraph in graph.subgraphs:
            self.write_subgraph(subgraph)
        self.finish_subgraph(graph)

    def write_edges(self):
        self.start_edges()
        for edge in self.graph.edges:
            self.write_edge(edge)
        self.finish_edges()

    def start_graph(self):
        pass

    def start_subgraph(self, graph):
        pass

    def write_node(self, node):
        pass

    def start_edges(self):
        pass

    def write_edge(self, edge):
        pass

    def finish_edges(self):
        pass

    def finish_subgraph(self, graph):
        pass

    def finish_graph(self):
        pass


class TgfWriter(Writer):
    def __init__(self, graph, output=None, logger=None):
        Writer.__init__(
                self, graph,
                output=output,
                logger=logger)
        self.i = 1
        self.id_map = {}

    def write_node(self, node):
        self.write('%d %s' % (self.i, node.label))
        self.id_map[node] = self.i
        self.i += 1

    def start_edges(self):
        self.write('#')

    def write_edge(self, edge):
        flavor = 'U' if edge.flavor == 'uses' else 'D'
        self.write(
                '%s %s %s' %
                (self.id_map[edge.source], self.id_map[edge.target], flavor))


class DotWriter(Writer):
    def __init__(self, graph,
                 options=None, output=None, logger=None, tabstop=4):
        Writer.__init__(
                self, graph,
                output=output,
                logger=logger,
                tabstop=tabstop)
        options = options or []
        if graph.grouped:
            options += ['clusterrank="local"']
        self.options = ', '.join(options)
        self.grouped = graph.grouped

    def start_graph(self):
        self.write('digraph G {')
        self.write('    graph [' + self.options + '];')
        self.indent()

    def start_subgraph(self, graph):
        self.log('Start subgraph %s' % graph.label)
        # Name must begin with "cluster" to be recognized as a cluster by GraphViz.
        self.write(
                "subgraph cluster_%s {\n" % graph.id)
        self.indent()

        # translucent gray (no hue to avoid visual confusion with any
        # group of colored nodes)
        self.write(
            'graph [style="filled,rounded",'
            'fillcolor="#80808018", label="%s"];'
            % graph.label)

    def finish_subgraph(self, graph):
        self.log('Finish subgraph %s' % graph.label)
        # terminate previous subgraph
        self.dedent()
        self.write('}')

    def write_node(self, node):
        self.log('Write node %s' % node.label)
        self.write(
            '%s [label="%s", style="filled", fillcolor="%s",'
            ' fontcolor="%s", group="%s"];'
            % (
                node.id, node.label,
                node.fill_color, node.text_color, node.group))

    def write_edge(self, edge):
        source = edge.source
        target = edge.target
        color  = edge.color
        if edge.flavor == 'defines':
            self.write(
                '    %s -> %s [style="dashed",'
                ' color="%s"];'
                % (source.id, target.id, color))
        else: # edge.flavor == 'uses':
            self.write(
                '    %s -> %s [style="solid",'
                ' color="%s"];'
                % (source.id, target.id, color))

    def finish_graph(self):
        self.write('}')  # terminate "digraph G {"


class YedWriter(Writer):
    def __init__(self, graph, output=None, logger=None, tabstop=2):
        Writer.__init__(
                self, graph,
                output=output,
                logger=logger,
                tabstop=tabstop)
        self.grouped = graph.grouped
        self.indent_level = 0
        self.edge_id = 0

    def start_graph(self):
        self.write('<?xml version="1.0" encoding="UTF-8" standalone="no"?>')
        self.write(
                '<graphml xmlns="http://graphml.graphdrawing.org/xmlns"'
                ' xmlns:java='
                '"http://www.yworks.com/xml/yfiles-common/1.0/java"'
                ' xmlns:sys='
                '"http://www.yworks.com/xml/yfiles-common/markup/primitives'
                '/2.0" xmlns:x="http://www.yworks.com/xml/yfiles-common/'
                'markup/2.0" xmlns:xsi="http://www.w3.org/2001/'
                'XMLSchema-instance" xmlns:y="http://www.yworks.com/xml/'
                'graphml" xmlns:yed="http://www.yworks.com/xml/yed/3"'
                ' xsi:schemaLocation="http://graphml.graphdrawing.org/xmlns'
                ' http://www.yworks.com/xml/schema/graphml/1.1/'
                'ygraphml.xsd">')
        self.indent()
        self.write('<key for="node" id="d0" yfiles.type="nodegraphics"/>')
        self.write('<key for="edge" id="d1" yfiles.type="edgegraphics"/>')
        self.write('<graph edgedefault="directed" id="Graph">')
        self.indent()

    def start_subgraph(self, graph):
        self.log('Start subgraph %s' % graph.label)

        self.write('<node id="%s:" yfiles.foldertype="group">' % graph.id)
        self.indent()
        self.write('<data key="d0">')
        self.indent()
        self.write('<y:ProxyAutoBoundsNode>')
        self.indent()
        self.write('<y:Realizers active="0">')
        self.indent()
        self.write('<y:GroupNode>')
        self.indent()
        self.write('<y:Fill color="#CCCCCC" transparent="false"/>')
        self.write('<y:NodeLabel modelName="internal" modelPosition="t" '
                   'alignment="right">%s</y:NodeLabel>'
                   % graph.label)
        self.write('<y:Shape type="roundrectangle"/>')
        self.dedent()
        self.write('</y:GroupNode>')
        self.dedent()
        self.write('</y:Realizers>')
        self.dedent()
        self.write('</y:ProxyAutoBoundsNode>')
        self.dedent()
        self.write('</data>')
        self.write('<graph edgedefault="directed" id="%s::">' % graph.id)
        self.indent()

    def finish_subgraph(self, graph):
        self.log('Finish subgraph %s' % graph.label)
        self.dedent()
        self.write('</graph>')
        self.dedent()
        self.write('</node>')

    def write_node(self, node):
        self.log('Write node %s' % node.label)
        width = 20 + 10*len(node.label)
        self.write('<node id="%s">' % node.id)
        self.indent()
        self.write('<data key="d0">')
        self.indent()
        self.write('<y:ShapeNode>')
        self.indent()
        self.write('<y:Geometry height="%s" width="%s"/>' % ("30", width))
        self.write('<y:Fill color="%s" transparent="false"/>'
                   % node.fill_color)
        self.write('<y:BorderStyle color="#000000" type="line" '
                   'width="1.0"/>')
        self.write('<y:NodeLabel>%s</y:NodeLabel>'
                   % node.label)
        self.write('<y:Shape type="ellipse"/>')
        self.dedent()
        self.write('</y:ShapeNode>')
        self.dedent()
        self.write('</data>')
        self.dedent()
        self.write('</node>')

    def write_edge(self, edge):
        self.edge_id += 1
        source = edge.source
        target = edge.target
        self.write(
                '<edge id="%s" source="%s" target="%s">'
                % (self.edge_id, source.id, target.id))
        self.indent()
        self.write('<data key="d1">')
        self.indent()
        self.write('<y:PolyLineEdge>')
        self.indent()
        if edge.flavor == 'defines':
            self.write('<y:LineStyle color="%s" '
                       'type="dashed" width="1.0"/>'
                       % edge.color)
        else:
            self.write('<y:LineStyle color="%s" '
                       'type="line" width="1.0"/>'
                       % edge.color)
        self.write('<y:Arrows source="none" target="standard"/>')
        self.write('<y:BendStyle smoothed="true"/>')
        self.dedent()
        self.write('</y:PolyLineEdge>')
        self.dedent()
        self.write('</data>')
        self.dedent()
        self.write('</edge>')

    def finish_graph(self):
        self.dedent(2)
        self.write('  </graph>')
        self.dedent()
        self.write('</graphml>')
