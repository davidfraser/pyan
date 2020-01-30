#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
    pyan.py - Generate approximate call graphs for Python programs.

    This program takes one or more Python source files, does a superficial
    analysis, and constructs a directed graph of the objects in the combined
    source, and how they define or use each other.  The graph can be output
    for rendering by e.g. GraphViz or yEd.
"""

import logging
from glob import glob
from argparse import ArgumentParser

from .analyzer import CallGraphVisitor
from .visgraph import VisualGraph
from .writers import TgfWriter, DotWriter, YedWriter

def main():
    usage = """%(prog)s FILENAME... [--dot|--tgf|--yed]"""
    desc = ('Analyse one or more Python source files and generate an'
            'approximate call graph of the modules, classes and functions'
            ' within them.')
    parser = ArgumentParser(usage=usage, description=desc)
    parser.add_argument("--dot",
                      action="store_true", default=False,
                      help="output in GraphViz dot format")
    parser.add_argument("--tgf",
                      action="store_true", default=False,
                      help="output in Trivial Graph Format")
    parser.add_argument("--yed",
                      action="store_true", default=False,
                      help="output in yEd GraphML Format")
    parser.add_argument("-f", "--file", dest="filename",
                      help="write graph to FILE", metavar="FILE", default=None)
    parser.add_argument("-l", "--log", dest="logname",
                      help="write log to LOG", metavar="LOG")
    parser.add_argument("-v", "--verbose",
                      action="store_true", default=False, dest="verbose",
                      help="verbose output")
    parser.add_argument("-V", "--very-verbose",
                      action="store_true", default=False, dest="very_verbose",
                      help="even more verbose output (mainly for debug)")
    parser.add_argument("-d", "--defines",
                      action="store_true", default=True, dest="draw_defines",
                      help="add edges for 'defines' relationships [default]")
    parser.add_argument("-n", "--no-defines",
                      action="store_false", default=True, dest="draw_defines",
                      help="do not add edges for 'defines' relationships")
    parser.add_argument("-u", "--uses",
                      action="store_true", default=True, dest="draw_uses",
                      help="add edges for 'uses' relationships [default]")
    parser.add_argument("-N", "--no-uses",
                      action="store_false", default=True, dest="draw_uses",
                      help="do not add edges for 'uses' relationships")
    parser.add_argument("-c", "--colored",
                      action="store_true", default=False, dest="colored",
                      help="color nodes according to namespace [dot only]")
    parser.add_argument("-G", "--grouped-alt",
                      action="store_true", default=False, dest="grouped_alt",
                      help="suggest grouping by adding invisible defines edges [only useful with --no-defines]")
    parser.add_argument("-g", "--grouped",
                      action="store_true", default=False, dest="grouped",
                      help="group nodes (create subgraphs) according to namespace [dot only]")
    parser.add_argument("-e", "--nested-groups",
                      action="store_true", default=False, dest="nested_groups",
                      help="create nested groups (subgraphs) for nested namespaces (implies -g) [dot only]")
    parser.add_argument("--dot-rankdir", default="TB", dest="rankdir",
                      help=(
                        "specifies the dot graph 'rankdir' property for "
                        "controlling the direction of the graph. "
                        "Allowed values: ['TB', 'LR', 'BT', 'RL']. "
                        "[dot only]"))
    parser.add_argument("-a", "--annotated",
                      action="store_true", default=False, dest="annotated",
                      help="annotate with module and source line number")
    parser.add_argument("path", nargs='*')

    args = parser.parse_args()
    filenames = [fn2 for fn in args.path for fn2 in glob(fn)]
    if len(args.path) == 0:
        parser.error('Need one or more filenames to process')

    if args.nested_groups:
        args.grouped = True

    graph_options = {
            'draw_defines': args.draw_defines,
            'draw_uses': args.draw_uses,
            'colored': args.colored,
            'grouped_alt' : args.grouped_alt,
            'grouped': args.grouped,
            'nested_groups': args.nested_groups,
            'annotated': args.annotated}

    # TODO: use an int argument for verbosity
    logger = logging.getLogger(__name__)
    if args.very_verbose:
        logger.setLevel(logging.DEBUG)
    elif args.verbose:
        logger.setLevel(logging.INFO)
    else:
        logger.setLevel(logging.WARN)
    logger.addHandler(logging.StreamHandler())
    if args.logname:
        handler = logging.FileHandler(args.logname)
        logger.addHandler(handler)

    v = CallGraphVisitor(filenames, logger)
    graph = VisualGraph.from_visitor(v, options=graph_options, logger=logger)

    if args.dot:
        writer = DotWriter(
                graph,
                options=['rankdir='+args.rankdir],
                output=args.filename,
                logger=logger)
        writer.run()

    if args.tgf:
        writer = TgfWriter(
                graph, output=args.filename, logger=logger)
        writer.run()

    if args.yed:
        writer = YedWriter(
                graph, output=args.filename, logger=logger)
        writer.run()


if __name__ == '__main__':
    main()
