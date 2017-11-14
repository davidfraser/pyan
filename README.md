# Pyan3: Offline call graph generator for Python 3

Generate approximate call graphs for Python programs.

Pyan takes one or more Python source files, performs a (rather superficial) static analysis, and constructs a directed graph of the objects in the combined source, and how they define or use each other. The graph can be output for rendering, mainly by GraphViz.

*And now it is available for Python 3!*

[![Example output](graph0.png "Example: GraphViz rendering of Pyan output (click for .svg)")](graph0.svg)

**Defines** relations are drawn with *dotted gray arrows*.

**Uses** relations are drawn with *black solid arrows*.

**Nodes** are always filled, and made translucent to clearly show any arrows passing underneath them. This is especially useful for large graphs with GraphViz's `fdp` filter. If colored output is not enabled, the fill is white.

In **node coloring**, the [HSL](https://en.wikipedia.org/wiki/HSL_and_HSV) color model is used. The **hue** is determined by the *top-level namespace* the node is in. The **lightness** is determined by *depth of namespace nesting*, with darker meaning more deeply nested. Saturation is constant. The spacing between different hues depends on the number of files analyzed; better results are obtained for fewer files.

**Groups** are filled with translucent gray to avoid clashes with any node color.

The nodes can be **annotated** by *filename and source line number* information.

## Note

The static analysis approach Pyan takes is different from running the code and seeing which functions are called and how often. There are various tools that will generate a call graph that way, usually using a debugger or profiling trace hooks, such as [Python Call Graph](https://pycallgraph.readthedocs.org/).

In Pyan3, the analyzer was ported from `compiler` ([good riddance](https://stackoverflow.com/a/909172)) to a combination of `ast` and `symtable`, and slightly extended.


# Usage

See `pyan --help`.

Example:

`pyan *.py --uses --no-defines --colored --grouped --annotated --dot >myuses.dot`

Then render using your favorite GraphViz filter, mainly `dot` or `fdp`:

`dot -Tsvg myuses.dot >myuses.svg`

#### Troubleshooting

If GraphViz says *trouble in init_rank*, try adding `-Gnewrank=true`, as in:

`dot -Gnewrank=true -Tsvg myuses.dot >myuses.svg`

Usually either old or new rank (but often not both) works; this is a long-standing GraphViz issue with complex graphs.


# Features

*Items tagged with ☆ are new in Pyan3.*

**Graph creation**:

 - Nodes for functions and classes
 - Edges for defines
 - Edges for uses
 - Grouping to represent defines, with or without nesting
 - Coloring of nodes by top-level namespace
   - Unlimited number of hues ☆

**Analysis**:

 - Name lookup across the given set of files
 - Nested function definitions
 - Nested class definitions ☆
 - Nested attribute accesses like `self.a.b` ☆
 - Assignment tracking with lexical scoping  
   - E.g. if `self.a = MyFancyClass()`, the analyzer knows that any references to `self.a` point to `MyFancyClass`
   - All binding forms are supported (assign, augassign, for, comprehensions, generator expressions) ☆  
     - Name clashes between `for` loop counter variables and functions or classes defined elsewhere no longer confuse Pyan.
 - Simple item-by-item tuple assignments like `x,y,z = a,b,c` ☆
 - Chained assignments `a = b = c` ☆
 - Local scope for lambda, listcomp, setcomp, dictcomp, genexpr ☆
   - Keep in mind that list comprehensions gained a local scope (being treated like a function) only in Python 3. Thus, Pyan3, when applied to legacy Python 2 code, will give subtly wrong results if the code uses list comprehensions.
 - Source filename and line number annotation ☆
   - The annotation is appended to the node label. If grouping is off, namespace is included in the annotation. If grouping is on, only source filename and line number information is included, because the group title already shows the namespace.

## TODO

 - This version is currently missing the PRs from [David Fraser's repo](https://github.com/davidfraser/pyan).
 - Get rid of `self.last_value`.  
   - Consider each specific kind of expression or statement being handled; get the relevant info directly (or by a more controlled kind of recursion) instead of `self.visit()`.
   - At some point, may need a second visitor class that is just a catch-all that extracts names, which is then applied to only relevant branches of the AST.

The analyzer **does not currently support**:

 - Tuples/lists as first-class values (will ignore any assignment of a tuple/list to a single name).
 - Starred assignment `a,*b,c = d,e,f,g,h` (will detect some item from the RHS).
 - Additional unpacking generalizations ([PEP 448](https://www.python.org/dev/peps/pep-0448/), Python 3.5+).
 - Type hints ([PEP 484](https://www.python.org/dev/peps/pep-0484/), Python 3.5+).
 - Use of `self` is detected by the literal name `self`, not by capturing the name of the first argument of a method definition.
 - Async definitions are detected, but passed through to the corresponding non-async analyzers; could be annotated.
 - Cython; could strip or comment out Cython-specific code as a preprocess step, then treat as Python (will need to be careful to get line numbers right).

# How it works

From the viewpoint of graphing the defines and uses relations, the interesting parts of the [AST](https://en.wikipedia.org/wiki/Abstract_syntax_tree) are bindings (defining new names, or assigning new values to existing names), and any name that appears in an `ast.Load` context (i.e. a use). The latter includes function calls; the function's name then appears in a load context inside the `ast.Call` node that represents the call site.

Bindings are tracked, with lexical scoping, to determine which type of object, or which function, each name points to at any given point in the source code being analyzed. This allows tracking things like:

```python
def some_func()
    pass

class MyClass:
    def __init__(self):
        self.f = some_func

    def dostuff(self)
        self.f()
```

By tracking the name `self.f`, the analyzer will see that `MyClass.dostuff()` uses `some_func()`.

The analyzer also needs to keep track of what type of object `self` currently points to. This is currently done by considering the literal `self` a special name in the lexical scope of the class.

Of course, this simple approach cannot correctly track cases where the current binding of `self.f` depends on the order in which the methods of the class are executed. To keep things simple, Pyan decides to ignore this complication, just reads through the code in a linear fashion (twice so that any forward-references are picked up), and uses the most recent binding that is currently in scope.

When a binding statement is encountered, the current namespace determines in which scope to store the new value for the name. Similarly, when encountering a use, the current namespace determines which object type or function to tag as the user.

# Authors

Original [pyan.py](https://github.com/ejrh/ejrh/blob/master/utils/pyan.py) by Edmund Horner. [Original post with explanation](http://ejrh.wordpress.com/2012/01/31/call-graphs-in-python-part-2/).

[Coloring and grouping](https://ejrh.wordpress.com/2012/08/18/coloured-call-graphs/) for GraphViz output by Juha Jeronen.

[Git repository cleanup](https://github.com/davidfraser/pyan/) by David Fraser.

This Python 3 port and refactoring to separate modules by Juha Jeronen.

# License

[GPL v2](LICENSE.md), as per [comments here](https://ejrh.wordpress.com/2012/08/18/coloured-call-graphs/).

