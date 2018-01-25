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
from optparse import OptionParser  # TODO: migrate to argparse

from .analyzer import CallGraphVisitor
from .visgraph import VisualGraph
from .writers import TgfWriter, DotWriter, YedWriter

def main():
    usage = """usage: %prog FILENAME... [--dot|--tgf|--yed]"""
    desc = ('Analyse one or more Python source files and generate an'
            'approximate call graph of the modules, classes and functions'
            ' within them.')
    parser = OptionParser(usage=usage, description=desc)
    parser.add_option("--dot",
                      action="store_true", default=False,
                      help="output in GraphViz dot format")
    parser.add_option("--tgf",
                      action="store_true", default=False,
                      help="output in Trivial Graph Format")
    parser.add_option("--yed",
                      action="store_true", default=False,
                      help="output in yEd GraphML Format")
    parser.add_option("-f", "--file", dest="filename",
                      help="write graph to FILE", metavar="FILE", default=None)
    parser.add_option("-l", "--log", dest="logname",
                      help="write log to LOG", metavar="LOG")
    parser.add_option("-v", "--verbose",
                      action="store_true", default=False, dest="verbose",
                      help="verbose output")
    parser.add_option("-V", "--very-verbose",
                      action="store_true", default=False, dest="very_verbose",
                      help="even more verbose output (mainly for debug)")
    parser.add_option("-d", "--defines",
                      action="store_true", default=True, dest="draw_defines",
                      help="add edges for 'defines' relationships [default]")
    parser.add_option("-n", "--no-defines",
                      action="store_false", default=True, dest="draw_defines",
                      help="do not add edges for 'defines' relationships")
    parser.add_option("-u", "--uses",
                      action="store_true", default=True, dest="draw_uses",
                      help="add edges for 'uses' relationships [default]")
    parser.add_option("-N", "--no-uses",
                      action="store_false", default=True, dest="draw_uses",
                      help="do not add edges for 'uses' relationships")
    parser.add_option("-c", "--colored",
                      action="store_true", default=False, dest="colored",
                      help="color nodes according to namespace [dot only]")
    parser.add_option("-G", "--grouped-alt",
                      action="store_true", default=False, dest="grouped_alt",
                      help="suggest grouping by adding invisible defines edges [only useful with --no-defines]")
    parser.add_option("-g", "--grouped",
                      action="store_true", default=False, dest="grouped",
                      help="group nodes (create subgraphs) according to namespace [dot only]")
    parser.add_option("-e", "--nested-groups",
                      action="store_true", default=False, dest="nested_groups",
                      help="create nested groups (subgraphs) for nested namespaces (implies -g) [dot only]")
    parser.add_option("--dot-rankdir", default="TB", dest="rankdir",
                      help=(
                        "specifies the dot graph 'rankdir' property for "
                        "controlling the direction of the graph. "
                        "Allowed values: ['TB', 'LR', 'BT', 'RL']. "
                        "[dot only]"))
    parser.add_option("-a", "--annotated",
                      action="store_true", default=False, dest="annotated",
                      help="annotate with module and source line number")

    options, args = parser.parse_args()
    filenames = [fn2 for fn in args for fn2 in glob(fn)]
    if len(args) == 0:
        parser.error('Need one or more filenames to process')

    if options.nested_groups:
        options.grouped = True

    graph_options = {
            'draw_defines': options.draw_defines,
            'draw_uses': options.draw_uses,
            'colored': options.colored,
            'grouped_alt' : options.grouped_alt,
            'grouped': options.grouped,
            'nested_groups': options.nested_groups,
            'annotated': options.annotated}

    # TODO: use an int argument for verbosity
    logger = logging.getLogger(__name__)
    if options.very_verbose:
        logger.setLevel(logging.DEBUG)
    elif options.verbose:
        logger.setLevel(logging.INFO)
    else:
        logger.setLevel(logging.WARN)
    logger.addHandler(logging.StreamHandler())
    if options.logname:
        handler = logging.FileHandler(options.logname)
        logger.addHandler(handler)

    v = CallGraphVisitor(filenames, logger)
    graph = VisualGraph.from_visitor(v, options=graph_options, logger=logger)

    if options.dot:
        writer = DotWriter(
                graph,
                options=['rankdir='+options.rankdir],
                output=options.filename,
                logger=logger)
        writer.run()

    if options.tgf:
        writer = TgfWriter(
                graph, output=options.filename, logger=logger)
        writer.run()

    if options.yed:
        writer = YedWriter(
                graph, output=options.filename, logger=logger)
        writer.run()


if __name__ == '__main__':
    main()
