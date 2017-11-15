#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
    pyan.py - Generate approximate call graphs for Python programs.

    This program takes one or more Python source files, does a superficial
    analysis, and constructs a directed graph of the objects in the combined
    source, and how they define or use each other.  The graph can be output
    for rendering by e.g. GraphViz or yEd.
"""

from glob import glob
from optparse import OptionParser  # TODO: migrate to argparse

from .common import MsgPrinter, MsgLevel
from .analyzer import CallGraphVisitor
from .graphgen import GraphGenerator

def main():
    usage = """usage: %prog FILENAME... [--dot|--tgf]"""
    desc = """Analyse one or more Python source files and generate an approximate call graph of the modules, classes and functions within them."""
    parser = OptionParser(usage=usage, description=desc)
    parser.add_option("--dot",
                      action="store_true", default=False,
                      help="output in GraphViz dot format")
    parser.add_option("--tgf",
                      action="store_true", default=False,
                      help="output in Trivial Graph Format")
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
                      help="annotate with module and source line number [dot only]")
    options, args = parser.parse_args()
    filenames = [fn2 for fn in args for fn2 in glob(fn)]
    if len(args) == 0:
        parser.error('Need one or more filenames to process')

    if options.nested_groups:
        options.grouped = True

    # TODO: use an int argument
    verbosity = MsgLevel.WARNING
    if options.very_verbose:
        verbosity = MsgLevel.DEBUG
    elif options.verbose:
        verbosity = MsgLevel.INFO
    m = MsgPrinter(verbosity)

    # Process the set of files, twiceso that any forward-references are picked up.
    v = CallGraphVisitor(filenames, msgprinter=m)
    for pas in range(2):
        for filename in filenames:
            m.message("========== pass %d, file '%s' ==========" % (pas+1, filename), level=MsgLevel.INFO)
            v.process(filename)
    v.postprocess()

    g = GraphGenerator(v, msgprinter=m)
    if options.dot:
        print(g.to_dot(draw_defines=options.draw_defines,
                       draw_uses=options.draw_uses,
                       colored=options.colored,
                       grouped=options.grouped,
                       nested_groups=options.nested_groups,
                       annotated=options.annotated,
                       rankdir=options.rankdir))
    if options.tgf:
        print(g.to_tgf(draw_defines=options.draw_defines,
                       draw_uses=options.draw_uses))


if __name__ == '__main__':
    main()
