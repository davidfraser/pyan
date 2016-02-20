import re
import logging
from colors import Colorizer


class GraphNode(object):
    """
    A node.

    flavor is meant to be used one day for things like 'source file', 'class',
    'function'...
    """
    def __init__(
            self, id, label='', flavor='',
            fill_color='', text_color='', group=''):
        self.id = id
        self.label = label
        self.flavor = ''
        self.fill_color = fill_color
        self.text_color = text_color
        self.group = group

    def __repr__(self):
        optionals = [
                repr(s) for s in [
                    self.label, self.flavor,
                    self.fill_color, self.text_color, self.group] if s]
        if optionals:
            return ('GraphNode(' + repr(self.id) +
                    ', ' + ', '.join(optionals)+')')
        else:
            return 'GraphNode(' + repr(self.id) + ')'


class GraphEdge(object):
    """
    An edge

    flavor is meant to be 'uses' or 'defines'
    """
    def __init__(self, source, target, flavor):
        self.source = source
        self.target = target
        self.flavor = flavor

    def __repr__(self):
        return (
                'Edge('+self.source.label+' '+self.flavor+' ' +
                self.target.label+')')


class Graph(object):
    def __init__(
            self, id, label, nodes=None, edges=None, subgraphs=None,
            grouped=False):
        self.id = id
        self.label = label
        self.nodes = nodes or []
        self.edges = edges or []
        self.subgraphs = subgraphs or []
        self.grouped = grouped

    @classmethod
    def from_visitor(cls, visitor, options=None, logger=None):
        colored = options.get('colored', False)

        nested = options.get('nested_groups', False)
        # enforce grouped when nested
        grouped = nested or options.get('grouped', False)

        draw_defines = options.get('draw_defines', False)
        draw_uses = options.get('draw_uses', False)

        logger = logger or logging.getLogger(__name__)
        colorizer = Colorizer(colored, logger)

        # collect and sort defined nodes
        visited_nodes = []
        for name in visitor.nodes:
            for node in visitor.nodes[name]:
                if node.defined:
                    visited_nodes.append(node)
        visited_nodes.sort()

        nodes_dict = dict()
        root_graph = cls('G', label='', grouped=grouped)
        subgraph = root_graph
        namespace_stack = []
        prev_namespace = ''
        for node in visited_nodes:
            logger.info('Looking at %s' % node.name)

            # Create the node itself and add it to nodes_dict
            fill_RGBA, text_RGB, idx = colorizer.make_colors(node)
            graph_node = GraphNode(
                    node.get_label(),
                    label=node.get_short_name(),
                    fill_color=fill_RGBA,
                    text_color=text_RGB,
                    group=idx)
            nodes_dict[node] = graph_node

            # new namespace? (NOTE: nodes sorted by namespace!)
            if grouped and node.namespace != prev_namespace:
                logger.info(
                        'New namespace %s, old was %s'
                        % (node.namespace, prev_namespace))

                label = node.namespace.replace('.', '__').replace('*', '')
                subgraph = cls(label, node.namespace)

                if nested:
                    # Pop the stack until the newly found namespace is within
                    # one of the parent namespaces, or until the stack runs out
                    # (i.e. this is a sibling).
                    j = len(namespace_stack) - 1
                    if j >= 0:
                        m = re.match(namespace_stack[j].label, node.namespace)
                        # The '.' check catches siblings in cases like
                        # MeshGenerator vs. Mesh.
                        while (
                                m is None or
                                m.end() == len(node.namespace) or
                                node.namespace[m.end()] != '.'):
                            namespace_stack.pop(j)
                            j -= 1
                            if j < 0:
                                break
                            m = re.match(
                                    namespace_stack[j].label, node.namespace)
                    parentgraph = namespace_stack[j] if j >= 0 else root_graph
                    parentgraph.subgraphs.append(subgraph)

                    namespace_stack.append(subgraph)
                else:
                    root_graph.subgraphs.append(subgraph)

                prev_namespace = node.namespace

            subgraph.nodes.append(graph_node)

        # Now add edges
        if draw_defines:
            for n in visitor.defines_edges:
                for n2 in visitor.defines_edges[n]:
                    if n2.defined and n2 != n:
                        root_graph.edges.append(
                                GraphEdge(
                                    nodes_dict[n],
                                    nodes_dict[n2],
                                    'defines'))

        if draw_uses:
            for n in visitor.uses_edges:
                for n2 in visitor.uses_edges[n]:
                    if n2.defined and n2 != n:
                        root_graph.edges.append(
                                GraphEdge(
                                    nodes_dict[n],
                                    nodes_dict[n2],
                                    'uses'))

        return root_graph
