"""
Simple sphinx extension that allows including callgraphs in documentation.

Example usage:

```
.. callgraph:: <function_name>


Options are

- **:no-groups:** (boolean flag): do not group
- **:no-defines:** (boolean flag): if to not draw edges that show which
  functions, methods and classes are defined by a class or module
- **:no-uses:** (boolean flag): if to not draw edges that show how a function
  uses other functions
- **:no-colors:** (boolean flag): if to not color in callgraph (default is
  coloring)
- **:nested-grops:** (boolean flag): if to group by modules and submodules
- **:annotated:** (boolean flag): annotate callgraph with file names
- **:direction:** (string): "horizontal" or "vertical" callgraph
- **:toctree:** (string): path to toctree (as used with autosummary) to link
  elements of callgraph to documentation (makes all nodes clickable)
- **:zoomable:** (boolean flag): enables users to zoom and pan callgraph
```
"""
import re
from typing import Any

from docutils.parsers.rst import directives
from sphinx.ext.graphviz import align_spec, figure_wrapper, graphviz
from sphinx.util.docutils import SphinxDirective

from pyan import create_callgraph


def direction_spec(argument: Any) -> str:
    return directives.choice(argument, ("vertical", "horizontal"))


class CallgraphDirective(SphinxDirective):

    # this enables content in the directive
    has_content = True

    option_spec = {
        # graphviz
        "alt": directives.unchanged,
        "align": align_spec,
        "caption": directives.unchanged,
        "name": directives.unchanged,
        "class": directives.class_option,
        # pyan
        "no-groups": directives.unchanged,
        "no-defines": directives.unchanged,
        "no-uses": directives.unchanged,
        "no-colors": directives.unchanged,
        "nested-groups": directives.unchanged,
        "annotated": directives.unchanged,
        "direction": direction_spec,
        "toctree": directives.unchanged,
        "zoomable": directives.unchanged,
    }

    def run(self):
        func_name = self.content[0]
        base_name = func_name.split(".")[0]
        if len(func_name.split(".")) == 1:
            func_name = None
        base_path = __import__(base_name).__path__[0]

        direction = "vertical"
        if "direction" in self.options:
            direction = self.options["direction"]
        dotcode = create_callgraph(
            filenames=f"{base_path}/**/*.py",
            root=base_path,
            function=func_name,
            namespace=base_name,
            format="dot",
            grouped="no-groups" not in self.options,
            draw_uses="no-uses" not in self.options,
            draw_defines="no-defines" not in self.options,
            nested_groups="nested-groups" in self.options,
            colored="no-colors" not in self.options,
            annotated="annotated" in self.options,
            rankdir={"horizontal": "LR", "vertical": "TB"}[direction],
        )
        node = graphviz()

        # insert link targets into groups: first insert link, then reformat link
        if "toctree" in self.options:
            path = self.options["toctree"].strip("/")
            # create raw link
            dotcode = re.sub(
                r'([\w\d]+)(\s.+), (style="filled")',
                r'\1\2, href="../' + path + r'/\1.html", target="_blank", \3',
                dotcode,
            )

            def create_link(dot_name):
                raw_link = re.sub(r"__(\w)", r".\1", dot_name)
                # determine if name this is a class by checking if its first letter is capital
                # (heuristic but should work almost always)
                splits = raw_link.rsplit(".", 2)
                if len(splits) > 1 and splits[-2][0].capitalize() == splits[-2][0]:
                    # is class
                    link = ".".join(splits[:-1]) + ".html#" + raw_link + '"'
                else:
                    link = raw_link + '.html"'
                return link

            dotcode = re.sub(
                r'(href="../' + path + r'/)(\w+)(\.html")',
                lambda m: m.groups()[0] + create_link(m.groups()[1]),
                dotcode,
            )

        node["code"] = dotcode
        node["options"] = {"docname": self.env.docname}
        if "graphviz_dot" in self.options:
            node["options"]["graphviz_dot"] = self.options["graphviz_dot"]
        if "layout" in self.options:
            node["options"]["graphviz_dot"] = self.options["layout"]
        if "alt" in self.options:
            node["alt"] = self.options["alt"]
        if "align" in self.options:
            node["align"] = self.options["align"]

        if "class" in self.options:
            classes = self.options["class"]
        else:
            classes = []
        if "zoomable" in self.options:
            if len(classes) == 0:
                classes = ["zoomable-callgraph"]
            else:
                classes.append("zoomable-callgraph")
        if len(classes) > 0:
            node["classes"] = classes

        if "caption" not in self.options:
            self.add_name(node)
            return [node]
        else:
            figure = figure_wrapper(self, node, self.options["caption"])
            self.add_name(figure)
            return [figure]


def setup(app):

    app.add_directive("callgraph", CallgraphDirective)
    app.add_js_file("https://cdn.jsdelivr.net/npm/svg-pan-zoom@3.6.1/dist/svg-pan-zoom.min.js")

    # script to find zoomable svgs
    script = """
    window.addEventListener('load', () => {
        Array.from(document.getElementsByClassName('zoomable-callgraph')).forEach(function(element) {
            svgPanZoom(element);
        });
    })
    """

    app.add_js_file(None, body=script)

    return {
        "version": "0.1",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
