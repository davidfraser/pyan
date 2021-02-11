#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from glob import glob
import io
from typing import List, Union

from .analyzer import CallGraphVisitor
from .main import main  # noqa: F401, for export only.
from .visgraph import VisualGraph
from .writers import DotWriter, HTMLWriter, SVGWriter

__version__ = "1.2.0"


# TODO: fix code duplication with main.py, should have just one implementation.
def create_callgraph(
    filenames: Union[List[str], str] = "**/*.py",
    root: str = None,
    function: Union[str, None] = None,
    namespace: Union[str, None] = None,
    format: str = "dot",
    rankdir: str = "LR",
    nested_groups: bool = True,
    draw_defines: bool = True,
    draw_uses: bool = True,
    colored: bool = True,
    grouped_alt: bool = False,
    annotated: bool = False,
    grouped: bool = True,
    max_iter: int = 1000,
) -> str:
    """
    create callgraph based on static code analysis

    Args:
        filenames: glob pattern or list of glob patterns
            to identify filenames to parse (`**` for multiple directories)
            example: **/*.py for all python files
        root: path to known root directory at which package root sits. Defaults to None, i.e. it will be inferred.
        function: if defined, function name to filter for, e.g. "my_module.my_function"
            to only include calls that are related to `my_function`
        namespace: if defined, namespace to filter for, e.g. "my_module", it is highly
            recommended to define this filter
        format: format to write callgraph to, of of "dot", "svg", "html". you need to have graphviz
            installed for svg or html output
        rankdir: direction of graph, e.g. "LR" for horizontal or "TB" for vertical
        nested_groups: if to group by modules and submodules
        draw_defines: if to draw defines edges (functions that are defines)
        draw_uses: if to draw uses edges (functions that are used)
        colored: if to color graph
        grouped_alt: if to use alternative grouping
        annotated: if to annotate graph with filenames
        grouped: if to group by modules
        max_iter: maximum number of iterations for filtering. Defaults to 1000.

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

    v = CallGraphVisitor(filenames, root=root)
    if function or namespace:
        if function:
            function_name = function.split(".")[-1]
            function_namespace = ".".join(function.split(".")[:-1])
            node = v.get_node(function_namespace, function_name)
        else:
            node = None
        v.filter(node=node, namespace=namespace, max_iter=max_iter)
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
