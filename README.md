# Pyan3

Offline call graph generator for Python 3

[![Build Status](https://travis-ci.com/edumco/pyan.svg?branch=master)](https://travis-ci.com/edumco/pyan)
[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2Fedumco%2Fpyan.svg?type=shield)](https://app.fossa.io/projects/git%2Bgithub.com%2Fedumco%2Fpyan?ref=badge_shield)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/7cba5ba5d3694a42a1252243e3634b5e)](https://www.codacy.com/manual/edumco/pyan?utm_source=github.com&utm_medium=referral&utm_content=edumco/pyan&utm_campaign=Badge_Grade)
![PyPI - Python Version](https://img.shields.io/pypi/pyversions/pyan3)

Pyan takes one or more Python source files, performs a (rather superficial) static analysis, and constructs a directed graph of the objects in the combined source, and how they define or use each other. The graph can be output for rendering by GraphViz or yEd.

This project has 2 official repositories:

- The original stable [davidfraser/pyan](https://github.com/davidfraser/pyan).
- The development repository [Technologicat/pyan](https://github.com/Technologicat/pyan)

> The PyPI package [pyan3](https://pypi.org/project/pyan3/) is built from development

## About

[![Example output](graph0.png "Example: GraphViz rendering of Pyan output (click for .svg)")](graph0.svg)

**Defines** relations are drawn with _dotted gray arrows_.

**Uses** relations are drawn with _black solid arrows_. Recursion is indicated by an arrow from a node to itself. [Mutual recursion](https://en.wikipedia.org/wiki/Mutual_recursion#Basic_examples) between nodes X and Y is indicated by a pair of arrows, one pointing from X to Y, and the other from Y to X.

**Nodes** are always filled, and made translucent to clearly show any arrows passing underneath them. This is especially useful for large graphs with GraphViz's `fdp` filter. If colored output is not enabled, the fill is white.

In **node coloring**, the [HSL](https://en.wikipedia.org/wiki/HSL_and_HSV) color model is used. The **hue** is determined by the _filename_ the node comes from. The **lightness** is determined by _depth of namespace nesting_, with darker meaning more deeply nested. Saturation is constant. The spacing between different hues depends on the number of files analyzed; better results are obtained for fewer files.

**Groups** are filled with translucent gray to avoid clashes with any node color.

The nodes can be **annotated** by _filename and source line number_ information.

## Note

The static analysis approach Pyan takes is different from running the code and seeing which functions are called and how often. There are various tools that will generate a call graph that way, usually using a debugger or profiling trace hooks, such as [Python Call Graph](https://pycallgraph.readthedocs.org/).

In Pyan3, the analyzer was ported from `compiler` ([good riddance](https://stackoverflow.com/a/909172)) to a combination of `ast` and `symtable`, and slightly extended.

# Install

    pip install pyan3

# Usage

See `pyan3 --help`.

Example:

`pyan *.py --uses --no-defines --colored --grouped --annotated --dot >myuses.dot`

Then render using your favorite GraphViz filter, mainly `dot` or `fdp`:

`dot -Tsvg myuses.dot >myuses.svg`

Or use directly

`pyan *.py --uses --no-defines --colored --grouped --annotated --svg >myuses.svg`

You can also export as an interactive HTML

`pyan *.py --uses --no-defines --colored --grouped --annotated --html > myuses.html`

Alternatively, you can call `pyan` from a script

```shell script
import pyan
from IPython.display import HTML
HTML(pyan.create_callgraph(filenames="**/*.py", format="html"))
```

#### Sphinx integration

You can integrate callgraphs into Sphinx.
Install graphviz (e.g. via `sudo apt-get install graphviz`) and modify `source/conf.py` so that

```
# modify extensions
extensions = [
  ...
  "sphinx.ext.graphviz"
  "pyan.sphinx",
]

# add graphviz options
graphviz_output_format = "svg"
```

Now, there is a callgraph directive which has all the options of the [graphviz directive](https://www.sphinx-doc.org/en/master/usage/extensions/graphviz.html)
and in addition:

- **:no-groups:** (boolean flag): do not group
- **:no-defines:** (boolean flag): if to not draw edges that show which functions, methods and classes are defined by a class or module
- **:no-uses:** (boolean flag): if to not draw edges that show how a function uses other functions
- **:no-colors:** (boolean flag): if to not color in callgraph (default is coloring)
- **:nested-grops:** (boolean flag): if to group by modules and submodules
- **:annotated:** (boolean flag): annotate callgraph with file names
- **:direction:** (string): "horizontal" or "vertical" callgraph
- **:toctree:** (string): path to toctree (as used with autosummary) to link elements of callgraph to documentation (makes all nodes clickable)
- **:zoomable:** (boolean flag): enables users to zoom and pan callgraph

Example to create a callgraph for the function `pyan.create_callgraph` that is
zoomable, is defined from left to right and links each node to the API documentation that
was created at the toctree path `api`.

```
.. callgraph:: pyan.create_callgraph
   :toctree: api
   :zoomable:
   :direction: horizontal
```

#### Troubleshooting

If GraphViz says _trouble in init_rank_, try adding `-Gnewrank=true`, as in:

`dot -Gnewrank=true -Tsvg myuses.dot >myuses.svg`

Usually either old or new rank (but often not both) works; this is a long-standing GraphViz issue with complex graphs.

## Too much detail?

If the graph is visually unreadable due to too much detail, consider visualizing only a subset of the files in your project. Any references to files outside the analyzed set will be considered as undefined, and will not be drawn.

Currently Pyan always operates at the level of individual functions and methods; an option to visualize only relations between namespaces may (or may not) be added in a future version.

# Features

_Items tagged with ☆ are new in Pyan3._

**Graph creation**:

- Nodes for functions and classes
- Edges for defines
- Edges for uses
  - This includes recursive calls ☆
- Grouping to represent defines, with or without nesting
- Coloring of nodes by filename
  - Unlimited number of hues ☆

**Analysis**:

- Name lookup across the given set of files
- Nested function definitions
- Nested class definitions ☆
- Nested attribute accesses like `self.a.b` ☆
- Inherited attributes ☆
  - Pyan3 looks up also in base classes when resolving attributes. In the old Pyan, calls to inherited methods used to be picked up by `contract_nonexistents()` followed by `expand_unknowns()`, but that often generated spurious uses edges (because the wildcard to `*.name` expands to `X.name` _for all_ `X` that have an attribute called `name`.).
- Resolution of `super()` based on the static type at the call site ☆
- MRO is (statically) respected in looking up inherited attributes and `super()` ☆
- Assignment tracking with lexical scoping
  - E.g. if `self.a = MyFancyClass()`, the analyzer knows that any references to `self.a` point to `MyFancyClass`
  - All binding forms are supported (assign, augassign, for, comprehensions, generator expressions, with) ☆
    - Name clashes between `for` loop counter variables and functions or classes defined elsewhere no longer confuse Pyan.
- `self` is defined by capturing the name of the first argument of a method definition, like Python does. ☆
- Simple item-by-item tuple assignments like `x,y,z = a,b,c` ☆
- Chained assignments `a = b = c` ☆
- Local scope for lambda, listcomp, setcomp, dictcomp, genexpr ☆
  - Keep in mind that list comprehensions gained a local scope (being treated like a function) only in Python 3. Thus, Pyan3, when applied to legacy Python 2 code, will give subtly wrong results if the code uses list comprehensions.
- Source filename and line number annotation ☆
  - The annotation is appended to the node label. If grouping is off, namespace is included in the annotation. If grouping is on, only source filename and line number information is included, because the group title already shows the namespace.

## TODO

- Determine confidence of detected edges (probability that the edge is correct). Start with a binary system, with only values 1.0 and 0.0.
  - A fully resolved reference to a name, based on lexical scoping, has confidence 1.0.
  - A reference to an unknown name has confidence 0.0.
  - Attributes:
    - A fully resolved reference to a known attribute of a known object has confidence 1.0.
    - A reference to an unknown attribute of a known object has confidence 1.0. These are mainly generated by imports, when the imported file is not in the analyzed set. (Does this need a third value, such as 0.5?)
    - A reference to an attribute of an unknown object has confidence 0.0.
  - A wildcard and its expansions have confidence 0.0.
  - Effects of binding analysis? The system should not claim full confidence in a bound value, unless it fully understands both the binding syntax and the value. (Note that this is very restrictive. A function call or a list in the expression for the value will currently spoil the full analysis.)
  - Confidence values may need updating in pass 2.
- Make the analyzer understand `del name` (probably seen as `isinstance(node.ctx, ast.Del)` in `visit_Name()`, `visit_Attribute()`)
- Prefix methods by class name in the graph; create a legend for annotations. See the discussion [here](https://github.com/johnyf/pyan/issues/4).
- Improve the wildcard resolution mechanism, see discussion [here](https://github.com/johnyf/pyan/issues/5).
  - Could record the namespace of the use site upon creating the wildcard, and check any possible resolutions against that (requiring that the resolved name is in scope at the use site)?
- Add an option to visualize relations only between namespaces, useful for large projects.
  - Scan the nodes and edges, basically generate a new graph and visualize that.
- Publish test cases.
- Get rid of `self.last_value`?
  - Consider each specific kind of expression or statement being handled; get the relevant info directly (or by a more controlled kind of recursion) instead of `self.visit()`.
  - At some point, may need a second visitor class that is just a catch-all that extracts names, which is then applied to only relevant branches of the AST.
  - On the other hand, maybe `self.last_value` is the simplest implementation that extracts a value from an expression, and it only needs to be used in a controlled manner (as `analyze_binding()` currently does); i.e. reset before visiting, and reset immediately when done.

The analyzer **does not currently support**:

- Tuples/lists as first-class values (currently ignores any assignment of a tuple/list to a single name).
  - Support empty lists, too (for resolving method calls to `.append()` and similar).
- Starred assignment `a,*b,c = d,e,f,g,h`
- Slicing and indexing in assignment (`ast.Subscript`)
- Additional unpacking generalizations ([PEP 448](https://www.python.org/dev/peps/pep-0448/), Python 3.5+).
  - Any **uses** on the RHS _at the binding site_ in all of the above are already detected by the name and attribute analyzers, but the binding information from assignments of these forms will not be recorded (at least not correctly).
- Enums; need to mark the use of any of their attributes as use of the Enum. Need to detect `Enum` in `bases` during analysis of ClassDef; then tag the class as an enum and handle differently.
- Resolving results of function calls, except for a very limited special case for `super()`.
  - Any binding of a name to a result of a function (or method) call - provided that the binding itself is understood by Pyan - will instead show in the output as binding the name to that function (or method). (This may generate some unintuitive uses edges in the graph.)
- Distinguishing between different Lambdas in the same namespace (to report uses of a particular `lambda` that has been stored in `self.something`).
- Type hints ([PEP 484](https://www.python.org/dev/peps/pep-0484/), Python 3.5+).
- Type inference for function arguments
  - Either of these two could be used to bind function argument names to the appropriate object types, avoiding the need for wildcard references (especially for attribute accesses on objects passed in as function arguments).
  - Type inference could run as pass 3, using additional information from the state of the graph after pass 2 to connect call sites to function definitions. Alternatively, no additional pass; store the AST nodes in the earlier pass. Type inference would allow resolving some wildcards by finding the method of the actual object instance passed in.
  - Must understand, at the call site, whether the first positional argument in the function def is handled implicitly or not. This is found by looking at the flavor of the Node representing the call target.
- Async definitions are detected, but passed through to the corresponding non-async analyzers; could be annotated.
- Cython; could strip or comment out Cython-specific code as a preprocess step, then treat as Python (will need to be careful to get line numbers right).

# How it works

From the viewpoint of graphing the defines and uses relations, the interesting parts of the [AST](https://en.wikipedia.org/wiki/Abstract_syntax_tree) are bindings (defining new names, or assigning new values to existing names), and any name that appears in an `ast.Load` context (i.e. a use). The latter includes function calls; the function's name then appears in a load context inside the `ast.Call` node that represents the call site.

Bindings are tracked, with lexical scoping, to determine which type of object, or which function, each name points to at any given point in the source code being analyzed. This allows tracking things like:

```python
def some_func():
    pass

class MyClass:
    def __init__(self):
        self.f = some_func

    def dostuff(self)
        self.f()
```

By tracking the name `self.f`, the analyzer will see that `MyClass.dostuff()` uses `some_func()`.

The analyzer also needs to keep track of what type of object `self` currently points to. In a method definition, the literal name representing `self` is captured from the argument list, as Python does; then in the lexical scope of that method, that name points to the current class (since Pyan cares only about object types, not instances).

Of course, this simple approach cannot correctly track cases where the current binding of `self.f` depends on the order in which the methods of the class are executed. To keep things simple, Pyan decides to ignore this complication, just reads through the code in a linear fashion (twice so that any forward-references are picked up), and uses the most recent binding that is currently in scope.

When a binding statement is encountered, the current namespace determines in which scope to store the new value for the name. Similarly, when encountering a use, the current namespace determines which object type or function to tag as the user.

# Authors

See [AUTHORS.md](AUTHORS.md).

# License

[GPL v2](LICENSE.md), as per [comments here](https://ejrh.wordpress.com/2012/08/18/coloured-call-graphs/).
