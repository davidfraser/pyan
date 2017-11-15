#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""The AST visitor.

Created on Mon Nov 13 03:33:00 2017

Original code by Edmund Horner.
Python 3 port by Juha Jeronen.
"""

import ast
import symtable
from .common import MsgPrinter, MsgLevel, \
                    format_alias, get_ast_node_name, sanitize_exprs, get_module_name, \
                    Node, Scope

# TODO: add Cython support (strip type annotations in a preprocess step, then treat as Python)
# TODO: built-in functions (range(), enumerate(), zip(), iter(), ...):
#       add to a special scope "built-in" in analyze_scopes() (or ignore altogether)
# TODO: support Node-ifying ListComp et al, List, Tuple
# TODO: make the analyzer smarter (see individual TODOs below)

# Note the use of the term "node" for two different concepts:
#
#  - AST nodes (the "node" argument of CallGraphVisitor.visit_*())
#
#  - The Node class that mainly stores auxiliary information about AST nodes,
#    for the purposes of generating the call graph.
#
#    Namespaces also get a Node (with no associated AST node).

# These tables were useful for porting the visitor to Python 3:
#
# https://docs.python.org/2/library/compiler.html#module-compiler.ast
# https://docs.python.org/3/library/ast.html#abstract-grammar
#
class CallGraphVisitor(ast.NodeVisitor):
    """A visitor that can be walked over a Python AST, and will derive
    information about the objects in the AST and how they use each other.

    A single CallGraphVisitor object can be run over several ASTs (from a
    set of source files).  The resulting information is the aggregate from
    all files.  This way use information between objects in different files
    can be gathered."""

    def __init__(self, filenames, msgprinter=None):
        if msgprinter is None:
            msgprinter = MsgPrinter()
        self.msgprinter = msgprinter

        # full module names for all given files
        self.module_names = {}
        self.module_to_filename = {}  # inverse mapping for recording which file each AST node came from
        for filename in filenames:
            mod_name = get_module_name(filename)
            short_name = mod_name.rsplit('.', 1)[-1]
            self.module_names[short_name] = mod_name
            self.module_to_filename[mod_name] = filename
        self.filenames = filenames

        # data gathered from analysis
        self.defines_edges = {}
        self.uses_edges = {}
        self.nodes = {}   # Node name: list of Node objects (in possibly different namespaces)
        self.scopes = {}  # fully qualified name of namespace: Scope object

        # current context for analysis
        self.module_name = None
        self.filename = None
        self.name_stack  = []  # for building namespace name, node naming
        self.scope_stack = []  # the Scope objects currently in scope
        self.class_stack = []  # Nodes for class definitions currently in scope
        self.context_stack = []  # for detecting which FunctionDefs are methods
        self.last_value  = None

    def process(self, filename):
        """Analyze the specified Python source file."""

        if filename not in self.filenames:
            raise ValueError("Filename '%s' has not been preprocessed (was not given to __init__, which got %s)" % (filename, self.filenames))
        with open(filename, "rt", encoding="utf-8") as f:
            content = f.read()
        self.filename = filename
        self.module_name = get_module_name(filename)
        self.analyze_scopes(content, filename)  # add to the currently known scopes
        self.visit(ast.parse(content, filename))
        self.module_name = None
        self.filename = None

    def postprocess(self):
        """Finalize the analysis."""

        self.expand_unknowns()
        self.contract_nonexistents()
        self.cull_inherited()

    ###########################################################################
    # visitor methods

    # Python docs:
    # https://docs.python.org/3/library/ast.html#abstract-grammar

    def visit_Module(self, node):
        self.msgprinter.message("Module", level=MsgLevel.DEBUG)

        # TODO: self.get_node() this too, and associate_node() to get the
        # source file information for annotated output?

        ns = self.module_name
        self.name_stack.append(ns)
        self.scope_stack.append(self.scopes[ns])
        self.context_stack.append("Module %s" % (ns))
        self.generic_visit(node)  # visit the **children** of node
        self.context_stack.pop()
        self.scope_stack.pop()
        self.name_stack.pop()
        self.last_value = None

    def visit_ClassDef(self, node):
        self.msgprinter.message("ClassDef %s" % (node.name), level=MsgLevel.DEBUG)

        from_node = self.get_current_namespace()
        ns = from_node.get_name()
        to_node = self.get_node(ns, node.name, node)
        if self.add_defines_edge(from_node, to_node):
            self.msgprinter.message("Def from %s to Class %s" % (from_node, to_node), level=MsgLevel.INFO)

        # The graph Node may have been created earlier by a FromImport,
        # in which case its AST node points to the site of the import.
        #
        # Change the AST node association of the relevant graph Node
        # to this AST node (the definition site) to get the correct
        # source line number information in annotated output.
        #
        self.associate_node(to_node, node, self.filename)

        # Bind the name specified by the AST node to the graph Node
        # in the current scope.
        #
        self.set_value(node.name, to_node)

        self.class_stack.append(to_node)
        self.name_stack.append(node.name)
        inner_ns = self.get_current_namespace().get_name()
        self.scope_stack.append(self.scopes[inner_ns])
        self.context_stack.append("ClassDef %s" % (node.name))

        for b in node.bases:  # this will mark uses from a derived class to its bases (via names appearing in a load context).
            self.visit(b)
        for stmt in node.body:
            self.visit(stmt)

        self.context_stack.pop()
        self.scope_stack.pop()
        self.name_stack.pop()
        self.class_stack.pop()

    def visit_FunctionDef(self, node):
        self.msgprinter.message("FunctionDef %s" % (node.name), level=MsgLevel.DEBUG)

#        # Place instance members at class level in the call graph
#        # TODO: brittle: breaks analysis if __init__ defines an internal helper class,
#        # because then the scope lookup will fail. Disabled this special handling for now.
#        #
#        # Any assignments in __init__ to self.anything will still be picked up
#        # correctly, because they use setattr.
#        #
#        if node.name == '__init__':
#            for d in node.args.defaults:
#                self.visit(d)
#            for d in node.args.kw_defaults:
#                self.visit(d)
#            for stmt in node.body:
#                self.visit(stmt)
#            return

        from_node = self.get_current_namespace()
        ns = from_node.get_name()
        to_node = self.get_node(ns, node.name, node)
        if self.add_defines_edge(from_node, to_node):
            self.msgprinter.message("Def from %s to Function %s" % (from_node, to_node), level=MsgLevel.INFO)

        self.associate_node(to_node, node, self.filename)
        self.set_value(node.name, to_node)

        # Decorators belong to the surrounding scope, so analyze them
        # before entering the function scope. This also grabs the
        # name representing "self" if this is a method definition.
        self_name = self.analyze_functiondef(node)

        self.name_stack.append(node.name)
        inner_ns = self.get_current_namespace().get_name()
        self.scope_stack.append(self.scopes[inner_ns])
        self.context_stack.append("FunctionDef %s" % (node.name))

        # self_name is just an ordinary name in the method namespace, except
        # that its value is implicitly set by Python when the method is called.
        #
        # Bind self_name in the function namespace to its initial value,
        # i.e. the current class. (Class, because Pyan cares only about
        # object types, not instances.)
        #
        # After this point, self_name behaves like any other name.
        #
        if self_name is not None:
            class_node = self.get_current_class()
            self.scopes[inner_ns].defs[self_name] = class_node
            self.msgprinter.message('Method def: setting self name "%s" to %s' % (self_name, class_node), level=MsgLevel.INFO)

        for d in node.args.defaults:
            self.visit(d)
        for d in node.args.kw_defaults:
            self.visit(d)
        for stmt in node.body:
            self.visit(stmt)

        self.context_stack.pop()
        self.scope_stack.pop()
        self.name_stack.pop()

    def analyze_functiondef(self, ast_node):
        """Helper for analyzing function definitions.

        Visit decorators, and if this is a method definition, capture the name
        of the first positional argument to denote "self", like Python does.
        Return the name representing self, or None if not applicable."""

        if not isinstance(ast_node, ast.FunctionDef):
            raise TypeError("Expected ast.FunctionDef; got %s" % (type(ast_node)))

        # Visit decorators
        self.last_value = None
        deco_names = []
        for deco in ast_node.decorator_list:
            self.visit(deco)  # capture function name of decorator (self.last_value hack)
            deco_node = self.last_value
            if isinstance(deco_node, Node):
                deco_names.append(deco_node.name)
            self.last_value = None

        # Get literal for "self", if applicable.
        #
        # - ignore static methods
        # - ignore functions defined inside methods (this new FunctionDef
        #   must be directly in a class namespace)
        #
        in_class_ns = self.context_stack[-1].startswith("ClassDef")
        if in_class_ns and "staticmethod" not in deco_names:
            # We can treat instance methods and class methods the same,
            # since Pyan is only interested in object types, not instances.
            all_args = ast_node.args  # args, vararg (*args), kwonlyargs, kwarg (**kwargs)
            posargs = all_args.args
            if len(posargs):
                self_name = posargs[0].arg
                return self_name

    def visit_AsyncFunctionDef(self, node):
        self.visit_FunctionDef(node)  # TODO: alias for now; tag async functions in output in a future version?

    # This gives lambdas their own namespaces in the graph;
    # if that is not desired, this method can be simply omitted.
    #
    # (The default visit() already visits all the children of a generic AST node
    #  by calling generic_visit(); and visit_Name() captures any uses inside the lambda.)
    #
    def visit_Lambda(self, node):
        self.msgprinter.message("Lambda", level=MsgLevel.DEBUG)
        def process():
            for d in node.args.defaults:
                self.visit(d)
            for d in node.args.kw_defaults:
                self.visit(d)
            self.visit(node.body)  # single expr
        self.with_scope("lambda", process)

        # Add a defines edge, which will mark the lambda as defined,
        # allowing any uses to other objects from inside the lambda body
        # to be visualized.
        #
        # All lambdas in the current ns will be grouped into a single node,
        # as they have no name. We create a namespace-like node that has
        # no associated AST node, as it does not represent any unique AST node.
        from_node = self.get_current_namespace()
        ns = from_node.get_name()
        to_node = self.get_node(ns, "lambda", None)
        if self.add_defines_edge(from_node, to_node):
            self.msgprinter.message("Def from %s to Lambda %s" % (from_node, to_node), level=MsgLevel.INFO)

        self.last_value = to_node  # Make this lambda node assignable to track its uses.

    def visit_Import(self, node):
        self.msgprinter.message("Import %s" % [format_alias(x) for x in node.names], level=MsgLevel.DEBUG)

        # TODO: add support for relative imports (path may be like "....something.something")
        # https://www.python.org/dev/peps/pep-0328/#id10
        # Do we need to? Seems that at least "from .foo import bar" works already?

        for import_item in node.names:
            src_name = import_item.name  # what is being imported
            tgt_name = import_item.asname if import_item.asname is not None else src_name  # under which name

            # mark the use site
            #
            from_node = self.get_current_namespace()      # where it is being imported to, i.e. the **user**
            to_node  = self.get_node('', tgt_name, node)  # the thing **being used** (under the asname, if any)
            if self.add_uses_edge(from_node, to_node):
                self.msgprinter.message("Use from %s to Import %s" % (from_node, to_node), level=MsgLevel.INFO)

            # bind asname in the current namespace to the imported module
            #
            # conversion: possible short name -> fully qualified name
            # (when analyzing a set of files in the same directory)
            if src_name in self.module_names:
                mod_name = self.module_names[src_name]
            else:
                mod_name = src_name
            tgt_module = self.get_node('', mod_name, node)
            self.set_value(tgt_name, tgt_module)

    def visit_ImportFrom(self, node):
        self.msgprinter.message("ImportFrom: from %s import %s" % (node.module, [format_alias(x) for x in node.names]), level=MsgLevel.DEBUG)

        tgt_name = node.module
        from_node = self.get_current_namespace()
        to_node = self.get_node('', tgt_name, node)  # module, in top-level namespace
        if self.add_uses_edge(from_node, to_node):
            self.msgprinter.message("Use from %s to ImportFrom %s" % (from_node, to_node), level=MsgLevel.INFO)

        if tgt_name in self.module_names:
            mod_name = self.module_names[tgt_name]
        else:
            mod_name = tgt_name

        for import_item in node.names:
            name = import_item.name
            new_name = import_item.asname if import_item.asname is not None else name
            tgt_id = self.get_node(mod_name, name, node)  # we imported the identifier name from the module mod_name
            self.set_value(new_name, tgt_id)
            self.msgprinter.message("From setting name %s to %s" % (new_name, tgt_id), level=MsgLevel.INFO)

#    # Edmund Horner's original post has info on what this fixed in Python 2.
#    # https://ejrh.wordpress.com/2012/01/31/call-graphs-in-python-part-2/
#    #
#    # Essentially, this should make '.'.join(...) see str.join.
#    #
#    # Python 3.4 does not have ast.Constant, but 3.6 does. Disabling for now.
#    # TODO: revisit this part after upgrading Python.
#    #
#    def visit_Constant(self, node):
#        self.msgprinter.message("Constant %s" % (node.value), level=MsgLevel.DEBUG)
#        t = type(node.value)
#        tn = t.__name__
#        self.last_value = self.get_node('', tn, node)

    def resolve_attribute(self, ast_node):
        """Resolve an ast.Attribute.

        Nested attributes (a.b.c) are automatically handled by recursion.

        Return (obj,attrname), where obj is a Node (or None on lookup failure),
        and attrname is the attribute name.
        """

        if not isinstance(ast_node, ast.Attribute):
            raise TypeError("Expected ast.Attribute; got %s" % (type(ast_node)))

        self.msgprinter.message("Resolve %s.%s in context %s" % (get_ast_node_name(ast_node.value),
                                                                 ast_node.attr, type(ast_node.ctx)),
                                                                level=MsgLevel.DEBUG)

        # Resolve nested attributes
        #
        # In pseudocode, e.g. "a.b.c" is represented in the AST as:
        #    ast.Attribute(attr=c, value=ast.Attribute(attr=b, value=a))
        #
        if isinstance(ast_node.value, ast.Attribute):
            obj_node,attr_name = self.resolve_attribute(ast_node.value)

            if isinstance(obj_node, Node) and obj_node.namespace is not None:
                ns = obj_node.get_name()  # fully qualified namespace **of attr**
                if ns in self.scopes:  # imported modules not in the set of analyzed files are not seen by Pyan
                    sc = self.scopes[ns]
                    if attr_name in sc.defs:
                        self.msgprinter.message("Resolved to attr %s of %s" % (ast_node.attr, sc.defs[attr_name]),
                                                                level=MsgLevel.DEBUG)
                        return sc.defs[attr_name], ast_node.attr

            # It may happen that ast_node.value has no corresponding graph Node,
            # if this is a forward-reference, or a reference to a file
            # not in the analyzed set.
            #
            # In this case, return None for the object to let visit_Attribute()
            # add a wildcard reference to *.attr.
            #
            self.msgprinter.message("Unresolved, returning attr %s of unknown" % (ast_node.attr),
                                    level=MsgLevel.DEBUG)
            return None, ast_node.attr
        else:
            # Handle some constant types as a special case.
            # Needed particularly to detect str.join().
            #
            if isinstance(ast_node.value, (ast.Num, ast.Str)):  # TODO: other types?
                t = type(ast_node.value)
                tn = t.__name__
                # Create a namespace-like Node with no associated AST node.
                # Constants are builtins, so they should live in the
                # top-level namespace (same level as module names).
                #
                # Since get_node() creates only one node per unique
                # (namespace,name) pair, the AST node would anyway be
                # frozen to the first constant of any matching type that
                # the analyzer encountered in the analyzed source code,
                # which is not useful.
                obj_node = self.get_node('', tn, None)
            else:
                # Get the Node object corresponding to node.value in the current ns.
                #
                # (Using the current ns here is correct; this case only gets
                #  triggered when there are no more levels of recursion,
                #  and the leftmost name always resides in the current ns.)
                obj_node = self.get_value(get_ast_node_name(ast_node.value))  # resolves "self" if needed
            attr_name = ast_node.attr

        self.msgprinter.message("Resolved to attr %s of %s" % (attr_name, obj_node), level=MsgLevel.DEBUG)
        return obj_node, attr_name

    def get_attribute(self, ast_node):
        """Get value of an ast.Attribute.

        Return pair of Node objects (obj,attr), where each item can be None
        on lookup failure. (Object not known, or no Node value assigned
        to its attr.)
        """

        if not isinstance(ast_node.ctx, ast.Load):
            raise ValueError("Expected a load context, got %s" % (type(ast_node.ctx)))

        obj_node,attr_name = self.resolve_attribute(ast_node)

        if isinstance(obj_node, Node) and obj_node.namespace is not None:
            ns = obj_node.get_name()  # fully qualified namespace **of attr**

            # Handle some constant types as a special case.
            # Needed particularly to detect str.join().
            #
            # Any attribute is considered valid for these special types,
            # but only in a load context. (set_attribute() does not have this
            # special handling, by design.)
            #
            if ns in ("Num", "Str"):  # TODO: other types?
                return obj_node, self.get_node(ns, attr_name, None)

            if ns in self.scopes:
                sc = self.scopes[ns]
                if attr_name in sc.defs:
                    value_node = sc.defs[attr_name]
                else:
                    value_node = None
                return obj_node, value_node
        return obj_node, None  # here obj_node may be None

    def set_attribute(self, ast_node, new_value):
        """Assign the Node provided as new_value into the attribute described
        by the AST node ast_node. Return True if assignment was done,
        False otherwise."""

        if not isinstance(ast_node.ctx, ast.Store):
            raise ValueError("Expected a store context, got %s" % (type(ast_node.ctx)))

        if not isinstance(new_value, Node):
            return False

        obj_node,attr_name = self.resolve_attribute(ast_node)

        if isinstance(obj_node, Node) and obj_node.namespace is not None:
            ns = obj_node.get_name()  # fully qualified namespace **of attr**
            if ns in self.scopes:
                sc = self.scopes[ns]
                sc.defs[attr_name] = new_value
                return True
        return False

    # attribute access (node.ctx determines whether set (ast.Store) or get (ast.Load))
    def visit_Attribute(self, node):
        objname = get_ast_node_name(node.value)
        self.msgprinter.message("Attribute %s of %s in context %s" % (node.attr, objname, type(node.ctx)), level=MsgLevel.DEBUG)

        # TODO: self.last_value is a hack. Handle names in store context (LHS)
        # in analyze_binding(), so that visit_Attribute() only needs to handle
        # the load context (i.e. detect uses of the name).
        #
        if isinstance(node.ctx, ast.Store):
            new_value = self.last_value
            if self.set_attribute(node, new_value):
                self.msgprinter.message('setattr %s on %s to %s' % (node.attr, objname, new_value), level=MsgLevel.INFO)

        elif isinstance(node.ctx, ast.Load):
            obj_node,attr_node = self.get_attribute(node)

            # Both object and attr known.
            if isinstance(attr_node, Node):
                self.msgprinter.message('getattr %s on %s returns %s' % (node.attr, objname, attr_node), level=MsgLevel.INFO)

                # add uses edge
                from_node = self.get_current_namespace()
                if self.add_uses_edge(from_node, attr_node):
                    self.msgprinter.message("Use from %s to %s" % (from_node, attr_node), level=MsgLevel.INFO)

                # remove resolved wildcard from current site to <Node *.attr>
                if attr_node.namespace is not None:
                    self.remove_wild(from_node, attr_node, node.attr)

                self.last_value = attr_node

            # Object known, but attr unknown. Create node and add a uses edge.
            #
            # TODO: this is mainly useful for imports. Should probably disallow
            # creating new attribute nodes for other undefined attrs of known objs.
            #
            # E.g.
            #
            # import math  # create <Node math>
            # math.sin     # create <Node math.sin> (instead of <Node *.sin> even though math.py is not analyzed)
            #
            elif isinstance(obj_node, Node) and obj_node.namespace is not None:
                tgt_name = node.attr
                from_node = self.get_current_namespace()
                ns = obj_node.get_name()  # fully qualified namespace **of attr**
                to_node = self.get_node(ns, tgt_name, node)
                if self.add_uses_edge(from_node, to_node):
                    self.msgprinter.message("Use from %s to %s (target obj %s known but target attr %s not resolved; maybe fwd ref or unanalyzed import)" % (from_node, to_node, obj_node, node.attr), level=MsgLevel.INFO)

                # remove resolved wildcard from current site to <Node *.attr>
                self.remove_wild(from_node, obj_node, node.attr)

                self.last_value = to_node

            # Object unknown, add uses edge to a wildcard by attr name.
            else:
                tgt_name = node.attr
                from_node = self.get_current_namespace()
                to_node = self.get_node(None, tgt_name, node)
                if self.add_uses_edge(from_node, to_node):
                    self.msgprinter.message("Use from %s to %s (target obj %s not resolved; maybe fwd ref or unanalyzed import)" % (from_node, to_node, objname), level=MsgLevel.INFO)

                self.last_value = to_node

    # name access (node.ctx determines whether set (ast.Store) or get (ast.Load))
    def visit_Name(self, node):
        self.msgprinter.message("Name %s in context %s" % (node.id, type(node.ctx)), level=MsgLevel.DEBUG)

        # TODO: self.last_value is a hack. Handle names in store context (LHS)
        # in analyze_binding(), so that visit_Name() only needs to handle
        # the load context (i.e. detect uses of the name).
        #
        if isinstance(node.ctx, ast.Store):
            # when we get here, self.last_value has been set by visit_Assign()
            self.set_value(node.id, self.last_value)

        # A name in a load context is a use of the object the name points to.
        elif isinstance(node.ctx, ast.Load):
            tgt_name = node.id
            to_node = self.get_value(tgt_name)  # resolves "self" if needed
            current_class = self.get_current_class()
            if current_class is None or to_node is not current_class:  # add uses edge only if not pointing to "self"
                ###TODO if the name is a local variable (i.e. in the innermost scope), and
                ###has no known value, then don't try to create a Node for it.
                if not isinstance(to_node, Node):
                    to_node = self.get_node(None, tgt_name, node)  # namespace=None means we don't know the namespace yet

                from_node = self.get_current_namespace()
                if self.add_uses_edge(from_node, to_node):
                    self.msgprinter.message("Use from %s to Name %s" % (from_node, to_node), level=MsgLevel.INFO)

            self.last_value = to_node

    def analyze_binding(self, targets, values):
        """Generic handler for binding forms. Inputs must be sanitize_exprs()d."""

        # Before we begin analyzing the assignment, clean up any leftover self.last_value.
        #
        # (e.g. from any Name in load context (including function names in a Call)
        #  that did not assign anything.)
        #
        self.last_value = None

        # TODO: properly support tuple unpacking
        #
        #  - the problem is:
        #      a,*b,c = [1,2,3,4,5]  --> Name,Starred,Name = List
        #    so a simple analysis of the AST won't get us far here.
        #
        #  To fix this:
        #
        #  - find the index of Starred on the LHS
        #  - unpack the RHS into a tuple/list (if possible)
        #    - unpack just one level; the items may be tuples/lists and that's just fine
        #    - if not possible to unpack directly (e.g. enumerate(foo) is a **call**),
        #      don't try to be too smart; just do some generic fallback handling (or give up)
        #  - if RHS unpack successful:
        #    - map the non-starred items directly (one-to-one)
        #    - map the remaining sublist of the RHS to the Starred term
        #      - requires support for tuples/lists of AST nodes as values of Nodes
        #        - but generally, we need that anyway: consider self.a = (f, g, h)
        #          --> any use of self.a should detect the possible use of f, g, and h;
        #              currently this is simply ignored.
        #
        # TODO: support Additional Unpacking Generalizations (Python 3.6+):
        #       https://www.python.org/dev/peps/pep-0448/

        if len(targets) == len(values):  # handle correctly the most common trivial case "a1,a2,... = b1,b2,..."
            for tgt,value in zip(targets,values):
                self.visit(value)  # RHS -> set self.last_value to input for this tgt
                self.visit(tgt)    # LHS, name in a store context
                self.last_value = None
        else:  # FIXME: for now, do the wrong thing in the non-trivial case
            # old code, no tuple unpacking support
            for value in values:
                self.visit(value)  # set self.last_value to **something** on the RHS and hope for the best
            for tgt in targets:    # LHS, name in a store context
                self.visit(tgt)
            self.last_value = None

    def visit_Assign(self, node):
        # - chaining assignments like "a = b = c" produces multiple targets
        # - tuple unpacking works as a separate mechanism on top of that (see analyze_binding())
        #
        if len(node.targets) > 1:
            self.msgprinter.message("Assign (chained with %d outputs)" % (len(node.targets)), level=MsgLevel.DEBUG)

        values = sanitize_exprs(node.value)  # values is the same for each set of targets
        for targets in node.targets:
            targets = sanitize_exprs(targets)
            self.msgprinter.message("Assign %s %s" % ([get_ast_node_name(x) for x in targets],
                                                      [get_ast_node_name(x) for x in values]),
                                                     level=MsgLevel.DEBUG)
            self.analyze_binding(targets, values)

    def visit_AnnAssign(self, node):
        self.visit_Assign(self, node)  # TODO: alias for now; add the annotations to output in a future version?

    def visit_AugAssign(self, node):
        targets = sanitize_exprs(node.target)
        values = sanitize_exprs(node.value)  # values is the same for each set of targets

        self.msgprinter.message("AugAssign %s %s %s" % ([get_ast_node_name(x) for x in targets],
                                                        type(node.op),
                                                        [get_ast_node_name(x) for x in values]),
                                                       level=MsgLevel.DEBUG)

        # TODO: maybe no need to handle tuple unpacking in AugAssign? (but simpler to use the same implementation)
        self.analyze_binding(targets, values)

    # for() is also a binding form.
    #
    # (Without analyzing the bindings, we would get an unknown node for any
    #  use of the loop counter(s) in the loop body. This would have confusing
    #  consequences in the expand_unknowns() step, if the same name is
    #  in use elsewhere.)
    #
    def visit_For(self, node):
        self.msgprinter.message("For-loop", level=MsgLevel.DEBUG)

        targets = sanitize_exprs(node.target)
        values = sanitize_exprs(node.iter)
        self.analyze_binding(targets, values)

        for stmt in node.body:
            self.visit(stmt)
        for stmt in node.orelse:
            self.visit(stmt)

    def visit_AsyncFor(self, node):
        self.visit_For(node)  # TODO: alias for now; tag async for in output in a future version?

    def visit_ListComp(self, node):
        self.msgprinter.message("ListComp", level=MsgLevel.DEBUG)
        def process():
            self.visit(node.elt)
            self.analyze_generators(node.generators)
        self.with_scope("listcomp", process)

    def visit_SetComp(self, node):
        self.msgprinter.message("SetComp", level=MsgLevel.DEBUG)
        def process():
            self.visit(node.elt)
            self.analyze_generators(node.generators)
        self.with_scope("setcomp", process)

    def visit_DictComp(self, node):
        self.msgprinter.message("DictComp", level=MsgLevel.DEBUG)
        def process():
            self.visit(node.key)
            self.visit(node.value)
            self.analyze_generators(node.generators)
        self.with_scope("dictcomp", process)

    def visit_GeneratorExp(self, node):
        self.msgprinter.message("GeneratorExp", level=MsgLevel.DEBUG)
        def process():
            self.visit(node.elt)
            self.analyze_generators(node.generators)
        self.with_scope("genexpr", process)

    def analyze_generators(self, generators):
        """Analyze the generators in a comprehension form."""
        for gen in generators:
            # TODO: there's also an is_async field we might want to use in a future version.
            targets = sanitize_exprs(gen.target)
            values  = sanitize_exprs(gen.iter)
            self.analyze_binding(targets, values)

            for expr in gen.ifs:
                self.visit(expr)

    def visit_Call(self, node):
        self.msgprinter.message("Call %s" % (get_ast_node_name(node.func)), level=MsgLevel.DEBUG)

        for arg in node.args:
            self.visit(arg)
        for kw in node.keywords:
            self.visit(kw.value)

        # Visit the function name part last, so that inside a binding form,
        # it will be left standing as self.last_value.
        self.visit(node.func)

    ###########################################################################
    # Scope analysis

    def analyze_scopes(self, code, filename):
        """Gather lexical scope information."""

        # Below, ns is the fully qualified ("dotted") name of sc.
        #
        # Technically, the module scope is anonymous, but we treat it as if
        # it was in a namespace named after the module, to support analysis
        # of several files as a set (keeping their module-level definitions
        # in different scopes, as we should).
        #
        scopes = {}
        def process(parent_ns, table):
            sc = Scope(table)
            ns = "%s.%s" % (parent_ns, sc.name) if len(sc.name) else parent_ns
            scopes[ns] = sc
            for t in table.get_children():
                process(ns, t)
        process(self.module_name, symtable.symtable(code, filename, compile_type="exec"))

        # add to existing scopes (while not overwriting any existing definitions with None)
        for ns in scopes:
            if ns not in self.scopes:  # add new scope info
                self.scopes[ns] = scopes[ns]
            else:  # update existing scope info
                sc = scopes[ns]
                oldsc = self.scopes[ns]
                for name in sc.defs:
                    if name not in oldsc.defs:
                        oldsc.defs[name] = sc.defs[name]

        self.msgprinter.message("Scopes now: %s" % (self.scopes), level=MsgLevel.DEBUG)

    def with_scope(self, scopename, thunk):
        """Run thunk (0-argument function) with the scope stack augmented with an inner scope.
        Used to analyze lambda, listcomp et al. (The scope must still be present in self.scopes.)"""
        self.name_stack.append(scopename)
        inner_ns = self.get_current_namespace().get_name()
        if inner_ns not in self.scopes:
            raise ValueError("Unknown scope '%s'" % (inner_ns))
        self.scope_stack.append(self.scopes[inner_ns])
        thunk()
        self.scope_stack.pop()
        self.name_stack.pop()

    def get_current_class(self):
        """Return the node representing the current class, or None if not inside a class definition."""
        return self.class_stack[-1] if len(self.class_stack) else None

    def get_current_namespace(self):
        """Return a node representing the current namespace, based on self.name_stack."""

        # For a Node n representing a namespace:
        #   - n.namespace = parent namespaces (empty string if top level)
        #   - n.name      = name of this namespace
        #   - no associated AST node.

        assert len(self.name_stack)  # name_stack should never be empty (always at least module name)

        namespace = '.'.join(self.name_stack[0:-1])
        name = self.name_stack[-1]
        return self.get_node(namespace, name, None)

    def get_value(self, name):
        """Get the value of name in the current scope. Return the Node, or None if name is not set to a value."""

        # get the innermost scope that has name **and where name has a value**
        def find_scope(name):
            for sc in reversed(self.scope_stack):
                if name in sc.defs and sc.defs[name] is not None:
                    return sc

#        # If we wanted to get rid of a separate scope stack, we could do this:
#        def find_scope(name):
#            ns0 = self.get_current_namespace().get_name()
#            for j in range(ns0.count('.')+1):
#                ns = ns0.rsplit(".",j)[0]
#                if ns in self.scopes:
#                    sc = self.scopes[ns]
#                    if name in sc.defs and sc.defs[name] is not None:
#                        return sc

        sc = find_scope(name)
        if sc is not None:
            value = sc.defs[name]
            if isinstance(value, Node):
                self.msgprinter.message('Get %s in %s, found in %s, value %s' % (name, self.scope_stack[-1], sc, value), level=MsgLevel.INFO)
                return value
            else:
                self.msgprinter.message('Get %s in %s, found in %s: value %s is not a Node' % (name, self.scope_stack[-1], sc, value), level=MsgLevel.DEBUG)
        else:
            self.msgprinter.message('Get %s in %s: no Node value (or name not in scope)' % (name, self.scope_stack[-1]), level=MsgLevel.DEBUG)

    def set_value(self, name, value):
        """Set the value of name in the current scope. Value must be a Node."""

        # get the innermost scope that has name (should be the current scope unless name is a global)
        def find_scope(name):
            for sc in reversed(self.scope_stack):
                if name in sc.defs:
                    return sc

#        # If we wanted to get rid of a separate scope stack, we could do this:
#        def find_scope(name):
#            ns0 = self.get_current_namespace().get_name()
#            for j in range(ns0.count('.')+1):
#                ns = ns0.rsplit(".",j)[0]
#                if ns in self.scopes:
#                    sc = self.scopes[ns]
#                    if name in sc.defs:
#                        return sc

        sc = find_scope(name)
        if sc is not None:
            if isinstance(value, Node):
                sc.defs[name] = value
                self.msgprinter.message('Set %s in %s to %s' % (name, sc, value), level=MsgLevel.INFO)
            else:
                self.msgprinter.message('Set %s in %s: value %s is not a Node' % (name, sc, value), level=MsgLevel.DEBUG)
        else:
            self.msgprinter.message('Set: name %s not in scope' % (name), level=MsgLevel.DEBUG)

    ###########################################################################
    # Graph creation

    def get_node(self, namespace, name, ast_node=None):
        """Return the unique node matching the namespace and name.
        Creates a new node if one doesn't already exist.

        In CallGraphVisitor, always use get_node() to create nodes, because it
        also sets some auxiliary information. Do not call the Node constructor
        directly.
        """

        if name in self.nodes:
            for n in self.nodes[name]:
                if n.namespace == namespace:
                    return n

        # Try to figure out which source file this Node belongs to
        # (for annotated output).
        #
        # Other parts of the analyzer may change the filename later,
        # if a more authoritative source (e.g. a definition site) is found,
        # so the filenames should be trusted only after the analysis is
        # complete.
        #
        if namespace in self.module_to_filename:
            # If the namespace is one of the modules being analyzed,
            # the the Node belongs to the correponding file.
            filename = self.module_to_filename[namespace]
        else:  # Assume the Node belongs to the current file.
            filename = self.filename

        n = Node(namespace, name, ast_node, filename)

        if name in self.nodes:
            self.nodes[name].append(n)
        else:
            self.nodes[name] = [n]

        return n

    def associate_node(self, graph_node, ast_node, filename=None):
        """Change the AST node (and optionally filename) mapping of a graph node.

        This is useful for generating annotated output with source filename
        and line number information.

        Sometimes a function in the analyzed code is first seen in a FromImport
        before its definition has been analyzed. The namespace can be deduced
        correctly already at that point, but the source line number information
        has to wait until the actual definition is found (because the line
        number is contained in the AST node). However, a graph Node must be
        created immediately when the function is first encountered, in order
        to have a Node that can act as a "uses" target (namespaced correctly,
        to avoid the over-reaching unknowns expansion in cases where it is
        not needed).

        This method re-associates the given graph Node with a different
        AST node, which allows updating the context when the definition
        of a function or class is encountered."""
        graph_node.ast_node = ast_node
        if filename is not None:
            graph_node.filename = filename

    def add_defines_edge(self, from_node, to_node):
        """Add a defines edge in the graph between two nodes.
        N.B. This will mark both nodes as defined."""

        if from_node not in self.defines_edges:
            self.defines_edges[from_node] = set()
        if to_node in self.defines_edges[from_node]:
            return False
        self.defines_edges[from_node].add(to_node)
        from_node.defined = True
        to_node.defined = True
        return True

    def add_uses_edge(self, from_node, to_node):
        """Add a uses edge in the graph between two nodes."""

        if from_node not in self.uses_edges:
            self.uses_edges[from_node] = set()
        if to_node in self.uses_edges[from_node]:
            return False
        self.uses_edges[from_node].add(to_node)

        # for pass 2: remove uses edge to any matching wildcard target node
        # if the given to_node has a known namespace.
        #
        # Prevents the spurious reference to MyClass.f in this example:
        #
        # class MyClass:
        #     def __init__(self):
        #         pass
        #     def f():
        #         pass
        #
        # def main():
        #     f()
        #
        # def f():
        #     pass
        #
        # (caused by reference to *.f in pass 1, combined with
        #  expand_unknowns() in postprocessing.)
        #
        # TODO: this can still get confused. The wildcard is removed if the
        # name of *any* resolved uses edge matches, whereas the wildcard
        # may represent several uses, to different objects.
        #
        if to_node.namespace is not None:
            self.remove_wild(from_node, to_node, to_node.name)

        return True

    def remove_wild(self, from_node, to_node, name):
        """Remove uses edge from from_node to wildcard *.name.

        This needs both to_node and name  because in case of a bound name
        (e.g. attribute lookup) the name field of the *target value* does not
        necessarily match the formal name in the wildcard.

        Used for cleaning up forward-references once resolved.
        This prevents spurious edges due to expand_unknowns()."""

        if from_node not in self.uses_edges:  # no uses edges to remove
            return

        # Here we may prefer to err in one of two ways:
        #
        #  a) A node seemingly referring to itself is actually referring
        #     to somewhere else that was not fully resolved, so don't remove
        #     the wildcard.
        #
        #     Example:
        #
        #         import sympy as sy
        #         def simplify(expr):
        #             sy.simplify(expr)
        #
        #     If the source file of sy.simplify is not included in the set of
        #     analyzed files, this will generate a reference to *.simplify,
        #     which is formally satisfied by this function itself.
        #
        #  b) A node seemingly referring to itself is actually referring
        #     to itself (it can be e.g. a recursive function). Remove the wildcard.
        #
        #     Bad example:
        #
        #         def f(count):
        #             if count > 0:
        #                 return 1 + f(count-1)
        #             return 0
        #
        #     (This example is bad, because visit_FunctionDef() will pick up
        #      the f in the top-level namespace, so no reference to *.f
        #      should be generated in this particular case.)
        #
        # We choose a).
        #
        if to_node == from_node:
            return

        matching_wilds = [n for n in self.uses_edges[from_node] if n.namespace is None and n.name == name]
        assert len(matching_wilds) < 2  # the set can have only one wild of matching name
        if len(matching_wilds):
            wild_node = matching_wilds[0]
            self.msgprinter.message("Use from %s to %s resolves %s; removing wildcard" % (from_node, to_node, wild_node), level=MsgLevel.INFO)
            self.uses_edges[from_node].remove(wild_node)

    ###########################################################################
    # Postprocessing

    def contract_nonexistents(self):
        """For all use edges to non-existent (i.e. not defined nodes) X.name, replace with edge to *.name."""

        # TODO: this doesn't actually replace, only adds new edges. Should we remove the corresponding old edges?

        new_uses_edges = []
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                if n2.namespace is not None and not n2.defined:
                    n3 = self.get_node(None, n2.name, n2.ast_node)
                    n3.defined = False
                    new_uses_edges.append((n, n3))
                    self.msgprinter.message("Contracting non-existent from %s to %s" % (n, n2), level=MsgLevel.INFO)

        for from_node, to_node in new_uses_edges:
            self.add_uses_edge(from_node, to_node)

    def expand_unknowns(self):
        """For each unknown node *.name, replace all its incoming edges with edges to X.name for all possible Xs.

        Also mark all unknown nodes as not defined."""

        new_defines_edges = []
        for n in self.defines_edges:
            for n2 in self.defines_edges[n]:
                if n2.namespace is None:
                    for n3 in self.nodes[n2.name]:
                        if n3.namespace is not None:
                            new_defines_edges.append((n, n3))

        for from_node, to_node in new_defines_edges:
            self.add_defines_edge(from_node, to_node)
            self.msgprinter.message("Expanding unknowns: new defines edge from %s to %s" % (from_node, to_node), level=MsgLevel.INFO)

        new_uses_edges = []
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                if n2.namespace is None:
                    for n3 in self.nodes[n2.name]:
                        if n3.namespace is not None:
                            new_uses_edges.append((n, n3))

        for from_node, to_node in new_uses_edges:
            self.add_uses_edge(from_node, to_node)
            self.msgprinter.message("Expanding unknowns: new uses edge from %s to %s" % (from_node, to_node), level=MsgLevel.INFO)

        for name in self.nodes:
            for n in self.nodes[name]:
                if n.namespace is None:
                    n.defined = False

    def cull_inherited(self):
        """For each use edge from W to X.name, if it also has an edge to W to Y.name where Y is used by X, then remove the first edge."""

        removed_uses_edges = []
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                inherited = False
                for n3 in self.uses_edges[n]:
                    if n3.name == n2.name and n2.namespace is not None and n3.namespace is not None and n3.namespace != n2.namespace:
                        if '.' in n2.namespace:
                            nsp2,p2 = n2.namespace.rsplit('.', 1)
                        else:
                            nsp2,p2 = '',n2.namespace
                        if '.' in n3.namespace:
                            nsp3,p3 = n3.namespace.rsplit('.', 1)
                        else:
                            nsp3,p3 = '',n3.namespace
                        pn2 = self.get_node(nsp2, p2, None)
                        pn3 = self.get_node(nsp3, p3, None)
                        if pn2 in self.uses_edges and pn3 in self.uses_edges[pn2]:  # remove the first edge W to X.name
#                        if pn3 in self.uses_edges and pn2 in self.uses_edges[pn3]:  # remove the second edge W to Y.name (TODO: mode to choose this)
                            inherited = True

                if inherited and n in self.uses_edges:
                    removed_uses_edges.append((n, n2))
                    self.msgprinter.message("Removing inherited edge from %s to %s" % (n, n2), level=MsgLevel.INFO)

        for from_node, to_node in removed_uses_edges:
            self.uses_edges[from_node].remove(to_node)
