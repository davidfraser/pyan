pyan - Static Analysis of function and method call dependencies
===============================================================

`pyan` is a Python module that performs static analysis of Python code
to determine a call dependency graph between functions and methods.
This is different from running the code and seeing which functions are
called and how often; there are various tools that will generate a call graph
in that way, usually using debugger or profiling trace hooks - for example:
https://pycallgraph.readthedocs.org/

This code was originally written by Edmund Horner, and then modified by Juha Jeronen.
See the notes at the end of this file for licensing info, the original blog posts,
and links to their repositories.

Command-line options
--------------------

*Output format* (one of these is required)

- `--dot` Output to GraphViz
- `--tgf` Output in Trivial Graph Format

*GraphViz only options*

- Color nodes automatically (`-c` or `--colored`).
  A HSL color model is used, picking the hue based on the top-level namespace (effectively, the module).
  The colors start out light, and then darken for each level of nesting.
  Seven different hues are available, cycled automatically.
- Group nodes in the same namespace (`-g` or `--grouped`, `-e` or `--nested-groups`).
  GraphViz clusters are used for this. The namespace name is used as the cluster label.
  Groups can be created as standalone (`-g` or `--grouped`, always inside top-level graph)
  or nested (`-e` or `--nested-groups`). The nested mode follows the namespace structure of the code.

*Generation options*

- Disable generation of links for “defines” relationships (`-n` or `--no-defines`).
  This can make the resulting graph look much clearer, when there are a lot of “uses” relationships.
  This is especially useful for layout with `fdp`.
  To enable (the default), use `-u` or `--defines`
- Disable generation of links for “uses” relationships (`-N` or `--no-uses`).
  Can be useful for visualizing just where functions are defined.
  To enable (the default), use `-u` or `--uses`

*General*

- `-v` or `--verbose` for verbose output
- `-h` or `--help` for help

Drawing Style
-------------

The “defines” relations are drawn with gray arrows,
so that it’s easier to visually tell them apart from the “uses” relations
when there are a lot of edges of both types in the graph.

Nodes are always filled (white if color disabled), and made translucent to clearly show arrows passing underneath them.
This is useful for large graphs with the fdp filter.

Original blog posts
-------------------

- https://ejrh.wordpress.com/2011/12/23/call-graphs-in-python/
- https://ejrh.wordpress.com/2012/01/31/call-graphs-in-python-part-2/
- https://ejrh.wordpress.com/2012/08/18/coloured-call-graphs/


Original source repositories
----------------------------

- Edmund Horner's original code is now best found in his github repository at:
  https://github.com/ejrh/ejrh/blob/master/utils/pyan.py.
- Juha Jeronen's repository is at:
  https://yousource.it.jyu.fi/jjrandom2/miniprojects/blobs/master/refactoring/
- Daffyd Crosby has also made a repository with both versions, but with two files and no history:
  https://github.com/dafyddcrosby/pyan
- Since both original repositories have lots of other software,
  I've made this clean version combining their contributions into my own repository just for pyan.
  This contains commits filtered out of their original repositories, and reordered into a logical sequence:
  https://github.com/davidfraser/pyan

Licensing
---------

This code is made available under the GNU GPL, v2. See the LICENSE.md file,
or consult https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html for more information.

