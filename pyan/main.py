#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
    pyan.py - Generate approximate call graphs for Python programs.

    This program takes one or more Python source files, does a superficial
    analysis, and constructs a directed graph of the objects in the combined
    source, and how they define or use each other.  The graph can be output
    for rendering by e.g. GraphViz or yEd.
"""

from argparse import ArgumentParser
from glob import glob
import logging
import os

from .analyzer import CallGraphVisitor
from .visgraph import VisualGraph
from .writers import DotWriter, HTMLWriter, SVGWriter, TgfWriter, YedWriter


def main(cli_args=None):
    usage = """%(prog)s FILENAME... [--dot|--tgf|--yed|--svg|--html]"""
    desc = (
        "Analyse one or more Python source files and generate an"
        "approximate call graph of the modules, classes and functions"
        " within them."
    )

    parser = ArgumentParser(usage=usage, description=desc)

    parser.add_argument("--dot", action="store_true", default=False, help="output in GraphViz dot format")

    parser.add_argument("--tgf", action="store_true", default=False, help="output in Trivial Graph Format")

    parser.add_argument("--svg", action="store_true", default=False, help="output in SVG Format")

    parser.add_argument("--html", action="store_true", default=False, help="output in HTML Format")

    parser.add_argument("--yed", action="store_true", default=False, help="output in yEd GraphML Format")

    parser.add_argument("--file", dest="filename", help="write graph to FILE", metavar="FILE", default=None)

    parser.add_argument("--namespace", dest="namespace", help="filter for NAMESPACE", metavar="NAMESPACE", default=None)

    parser.add_argument("--function", dest="function", help="filter for FUNCTION", metavar="FUNCTION", default=None)

    parser.add_argument("-l", "--log", dest="logname", help="write log to LOG", metavar="LOG")

    parser.add_argument("-v", "--verbose", action="store_true", default=False, dest="verbose", help="verbose output")

    parser.add_argument(
        "-V",
        "--very-verbose",
        action="store_true",
        default=False,
        dest="very_verbose",
        help="even more verbose output (mainly for debug)",
    )

    parser.add_argument(
        "-d",
        "--defines",
        action="store_true",
        dest="draw_defines",
        help="add edges for 'defines' relationships [default]",
    )

    parser.add_argument(
        "-n",
        "--no-defines",
        action="store_false",
        default=True,
        dest="draw_defines",
        help="do not add edges for 'defines' relationships",
    )

    parser.add_argument(
        "-u",
        "--uses",
        action="store_true",
        default=True,
        dest="draw_uses",
        help="add edges for 'uses' relationships [default]",
    )

    parser.add_argument(
        "-N",
        "--no-uses",
        action="store_false",
        default=True,
        dest="draw_uses",
        help="do not add edges for 'uses' relationships",
    )

    parser.add_argument(
        "-c",
        "--colored",
        action="store_true",
        default=False,
        dest="colored",
        help="color nodes according to namespace [dot only]",
    )

    parser.add_argument(
        "-G",
        "--grouped-alt",
        action="store_true",
        default=False,
        dest="grouped_alt",
        help="suggest grouping by adding invisible defines edges [only useful with --no-defines]",
    )

    parser.add_argument(
        "-g",
        "--grouped",
        action="store_true",
        default=False,
        dest="grouped",
        help="group nodes (create subgraphs) according to namespace [dot only]",
    )

    parser.add_argument(
        "-e",
        "--nested-groups",
        action="store_true",
        default=False,
        dest="nested_groups",
        help="create nested groups (subgraphs) for nested namespaces (implies -g) [dot only]",
    )

    parser.add_argument(
        "--dot-rankdir",
        default="TB",
        dest="rankdir",
        help=(
            "specifies the dot graph 'rankdir' property for "
            "controlling the direction of the graph. "
            "Allowed values: ['TB', 'LR', 'BT', 'RL']. "
            "[dot only]"
        ),
    )

    parser.add_argument(
        "-a",
        "--annotated",
        action="store_true",
        default=False,
        dest="annotated",
        help="annotate with module and source line number",
    )

    parser.add_argument(
        "--root",
        default=None,
        dest="root",
        help="Package root directory. Is inferred by default.",
    )

    known_args, unknown_args = parser.parse_known_args(cli_args)

    filenames = [fn2 for fn in unknown_args for fn2 in glob(fn, recursive=True)]

    # determine root
    if known_args.root is not None:
        root = os.path.abspath(known_args.root)
    else:
        root = None

    if len(unknown_args) == 0:
        parser.error("Need one or more filenames to process")
    elif len(filenames) == 0:
        parser.error("No files found matching given glob: %s" % " ".join(unknown_args))

    if known_args.nested_groups:
        known_args.grouped = True

    graph_options = {
        "draw_defines": known_args.draw_defines,
        "draw_uses": known_args.draw_uses,
        "colored": known_args.colored,
        "grouped_alt": known_args.grouped_alt,
        "grouped": known_args.grouped,
        "nested_groups": known_args.nested_groups,
        "annotated": known_args.annotated,
    }

    # TODO: use an int argument for verbosity
    logger = logging.getLogger(__name__)

    if known_args.very_verbose:
        logger.setLevel(logging.DEBUG)

    elif known_args.verbose:
        logger.setLevel(logging.INFO)

    else:
        logger.setLevel(logging.WARN)

    logger.addHandler(logging.StreamHandler())

    if known_args.logname:
        handler = logging.FileHandler(known_args.logname)
        logger.addHandler(handler)

    v = CallGraphVisitor(filenames, logger, root=root)

    if known_args.function or known_args.namespace:

        if known_args.function:
            function_name = known_args.function.split(".")[-1]
            namespace = ".".join(known_args.function.split(".")[:-1])
            node = v.get_node(namespace, function_name)

        else:
            node = None

        v.filter(node=node, namespace=known_args.namespace)

    graph = VisualGraph.from_visitor(v, options=graph_options, logger=logger)

    writer = None

    if known_args.dot:
        writer = DotWriter(graph, options=["rankdir=" + known_args.rankdir], output=known_args.filename, logger=logger)

    if known_args.html:
        writer = HTMLWriter(graph, options=["rankdir=" + known_args.rankdir], output=known_args.filename, logger=logger)

    if known_args.svg:
        writer = SVGWriter(graph, options=["rankdir=" + known_args.rankdir], output=known_args.filename, logger=logger)

    if known_args.tgf:
        writer = TgfWriter(graph, output=known_args.filename, logger=logger)

    if known_args.yed:
        writer = YedWriter(graph, output=known_args.filename, logger=logger)

    if writer:
        writer.run()


if __name__ == "__main__":
    main()
