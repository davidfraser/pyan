#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import io
from glob import glob

from .main import main
from .analyzer import CallGraphVisitor
from .writers import SVGWriter, HTMLWriter, DotWriter
from .visgraph import VisualGraph

__version__ = "1.0.5"


def create_callgraph(
    filenames="**/*.py",
    function=None,
    namespace=None,
    format="dot",
    rankdir="LR",
    nested_groups=True,
    draw_defines=True,
    draw_uses=True,
    colored=True,
    grouped_alt=False,
    annotated=False,
    grouped=True,
):
    """
    create callgraph based on static code analysis

    Args:
        filenames (Union[List[str], str]): glob pattern or list of glob patterns
            to identify filenames to parse (`**` for multiple directories)
            example: **/*.py for all python files
        function (Union[str, None]): if defined, function name to filter for, e.g. "my_module.my_function"
            to only include calls that are related to `my_function`
        namespace (Union[str, None]): if defined, namespace to filter for, e.g. "my_module", it is highly
            recommended to define this filter
        format (str): format to write callgraph to, of of "dot", "svg", "html". you need to have graphviz
            installed for svg or html output
        rankdir (str): direction of graph, e.g. "LR" for horizontal or "TB" for vertical
        nested_groups (bool): if to group by modules and submodules
        draw_defines (bool): if to draw defines edges (functions that are defines)
        draw_uses (bool): if to draw uses edges (functions that are used)
        colored (bool): if to color graph
        grouped_alt (bool): if to use alternative grouping
        annotated (bool): if to annotate graph with filenames
        grouped (bool): if to group by modules

    Returns:
        str: callgraph
    """
    if isinstance(filenames, str):
        filenames = [filenames]
    filenames = [fn2 for fn in filenames for fn2 in glob(fn, recursive=True)]

    if nested_groups:
        grouped = True
    graph_options = {
        "draw_defines": draw_defines,
        "draw_uses": draw_uses,
        "colored": colored,
        "grouped_alt": grouped_alt,
        "grouped": grouped,
        "nested_groups": nested_groups,
        "annotated": annotated,
    }

    v = CallGraphVisitor(filenames)
    if function or namespace:
        if function:
            function_name = function.split(".")[-1]
            function_namespace = ".".join(function.split(".")[:-1])
            node = v.get_node(function_namespace, function_name)
        else:
            node = None
        v.filter(node=node, namespace=namespace)
    graph = VisualGraph.from_visitor(v, options=graph_options)

    stream = io.StringIO()
    if format == "dot":
        writer = DotWriter(graph, options=["rankdir=" + rankdir], output=stream)
        writer.run()

    elif format == "html":
        writer = HTMLWriter(graph, options=["rankdir=" + rankdir], output=stream)
        writer.run()

    elif format == "svg":
        writer = SVGWriter(graph, options=["rankdir=" + rankdir], output=stream)
        writer.run()
    else:
        raise ValueError(f"format {format} is unknown")

    return stream.getvalue()
