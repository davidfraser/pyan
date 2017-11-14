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
        self.ast_node_to_namespace = {}  # AST node: fully qualified name of namespace

        # current context for analysis
        self.module_name = None
        self.filename = None
        self.name_stack  = []  # for building namespace name, node naming
        self.scope_stack = []  # the Scope objects
        self.class_stack = []  # for resolving "self"
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
        self.ast_node_to_namespace[node] = ns  # must be added manually since we don't self.get_node() here
        self.generic_visit(node)  # visit the **children** of node
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
        for b in node.bases:
            self.visit(b)
        for stmt in node.body:
            self.visit(stmt)
        self.scope_stack.pop()
        self.name_stack.pop()
        self.class_stack.pop()

    def visit_FunctionDef(self, node):
        self.msgprinter.message("FunctionDef %s" % (node.name), level=MsgLevel.DEBUG)

#        # Place instance members at class level in the call graph
#        # TODO: brittle: breaks analysis if __init__ defines an internal helper class,
#        # because then the scope lookup will fail. Disabled this special handling for now.
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

        self.name_stack.append(node.name)
        inner_ns = self.get_current_namespace().get_name()
        self.scope_stack.append(self.scopes[inner_ns])
        for d in node.args.defaults:
            self.visit(d)
        for d in node.args.kw_defaults:
            self.visit(d)
        for stmt in node.body:
            self.visit(stmt)
        self.scope_stack.pop()
        self.name_stack.pop()

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

    def visit_Import(self, node):
        self.msgprinter.message("Import %s" % [format_alias(x) for x in node.names], level=MsgLevel.DEBUG)

        # TODO: add support for relative imports (path may be like "....something.something")
        # https://www.python.org/dev/peps/pep-0328/#id10

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

    # TODO: where are Constants used? (instead of Num, Str, ...)
    #
    # Edmund Horner's original post has info on what this fixed in Python 2.
    # https://ejrh.wordpress.com/2012/01/31/call-graphs-in-python-part-2/
    #
    # Essentially, this should make '.'.join(...) see str.join.
    #
    def visit_Constant(self, node):
        self.msgprinter.message("Constant %s" % (node.value), level=MsgLevel.DEBUG)
        t = type(node.value)
        tn = t.__name__
        self.last_value = self.get_node('', tn, node)

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
#                ns = self.ast_node_to_namespace[obj_node.ast_node] if isinstance(obj_node, Node) else None
#                sc = self.scopes[ns]
#                print(obj_node)
                ns = obj_node.namespace if len(obj_node.namespace) else self.module_name  # '' refers to module scope
                sc = self.scopes[ns]
                if attr_name in sc.defs:
                    return sc.defs[attr_name], ast_node.attr

            # It may happen that ast_node.value has no corresponding graph Node,
            # if this is a forward-reference, or a reference to a file
            # not in the analyzed set.
            #
            # In this case, return None for the object to let visit_Attribute()
            # add a wildcard reference to attr.
            #
            return None, ast_node.attr
        else:
            # Get the Node object corresponding to node.value in the current ns.
            #
            # (Using the current ns here is correct; this case only gets
            #  triggered when there are no more levels of recursion,
            #  and the leftmost name always resides in the current ns.)
            #
            obj_node = self.get_value(get_ast_node_name(ast_node.value))  # get_value() resolves "self" if needed.
            attr_name = ast_node.attr
        return obj_node, attr_name

    def get_attribute(self, ast_node):
        """Get value of an ast.Attribute.

        Return (obj,attr), where each element is a Node object, or None on
        lookup failure. (Object not known, or no Node value assigned to its attr.)
        """

        if not isinstance(ast_node.ctx, ast.Load):
            raise ValueError("Expected a load context, got %s" % (type(ast_node.ctx)))

        obj_node,attr_name = self.resolve_attribute(ast_node)

        # use the original AST node attached to the object's Node to look up the object's ns
        # TODO: do we need to resolve scopes here, or should the name always be directly in the object's ns?
        ns = self.ast_node_to_namespace[obj_node.ast_node] if isinstance(obj_node, Node) else None
        if ns in self.scopes:
            sc = self.scopes[ns]
            if attr_name in sc.defs:
                value_node = sc.defs[attr_name]
            else:
                value_node = None
            return obj_node,value_node
        else:
            return None,None

    def set_attribute(self, ast_node, new_value):
        """Assign the Node provided as new_value into the attribute described
        by the AST node ast_node. Return (obj,flag), where obj is a Node or None,
        and flag is True if assignment was done, False otherwise."""

        if not isinstance(ast_node.ctx, ast.Store):
            raise ValueError("Expected a store context, got %s" % (type(ast_node.ctx)))

        obj_node,attr_name = self.resolve_attribute(ast_node)

        if not isinstance(new_value, Node):
            return obj_node,False

        # use the original AST node attached to the object's Node to look up the object's ns
        ns = self.ast_node_to_namespace[obj_node.ast_node] if isinstance(obj_node, Node) else None
        if ns in self.scopes:
            sc = self.scopes[ns]
            sc.defs[attr_name] = new_value
            return obj_node,True
        return obj_node,False

    # attribute access (node.ctx determines whether set (ast.Store) or get (ast.Load))
    def visit_Attribute(self, node):
        self.msgprinter.message("Attribute %s of %s in context %s" % (node.attr, get_ast_node_name(node.value), type(node.ctx)), level=MsgLevel.DEBUG)

        if isinstance(node.ctx, ast.Store):
            obj_node,written = self.set_attribute(node, self.last_value)
            if written:
                self.msgprinter.message('setattr %s on %s to %s' % (node.attr, get_ast_node_name(node.value), self.last_value), level=MsgLevel.INFO)

        elif isinstance(node.ctx, ast.Load):
            obj_node,attr_node = self.get_attribute(node)
            if isinstance(obj_node, Node):
                self.msgprinter.message('getattr %s on %s returns %s' % (node.attr, get_ast_node_name(node.value), attr_node), level=MsgLevel.INFO)

                # remove resolved wildcard from current site to <*.attr>
                if obj_node.namespace is not None:
                    from_node = self.get_current_namespace()
                    self.remove_wild(from_node, obj_node, node.attr)

                self.last_value = attr_node
            else:  # unknown target obj, add uses edge to a wildcard
                tgt_name = node.attr
                from_node = self.get_current_namespace()
                to_node = self.get_node(None, tgt_name, node)
                if self.add_uses_edge(from_node, to_node):
                    self.msgprinter.message("Use from %s to Getattr %s (target object not resolved)" % (from_node, to_node), level=MsgLevel.INFO)

                self.last_value = to_node

    # name access (node.ctx determines whether set (ast.Store) or get (ast.Load))
    def visit_Name(self, node):
        self.msgprinter.message("Name %s in context %s" % (node.id, type(node.ctx)), level=MsgLevel.DEBUG)

        # TODO: handle this case in analyze_binding() and in visit_Attribute(),
        # so we won't need to care about names in a store context here.
        if isinstance(node.ctx, ast.Store):
            # when we get here, self.last_value has been set by visit_Assign()
            self.set_value(node.id, self.last_value)

        # any name in a load context = a use of the object the name currently points to
        elif isinstance(node.ctx, ast.Load):
            tgt_name = node.id
            to_node = self.get_value(tgt_name)
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
                self.visit(tgt)    # LHS
                self.last_value = None
        else:  # FIXME: for now, do the wrong thing in the non-trivial case
            # old code, no tuple unpacking support
            for value in values:
                self.visit(value)  # set self.last_value to **something** on the RHS and hope for the best
            for tgt in targets:    # LHS
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
    #  use of the loop counter(s) in the loop body. This can have confusing
    #  consequences in the expand_unknowns() step, if the same name is
    #  in use elsewhere. Thus, we treat for() properly, as a binding form.)
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
                for dn in sc.defs:
                    if dn not in oldsc.defs:
                        oldsc.defs[dn] = sc.defs[dn]

        self.msgprinter.message("Scopes now: %s" % (self.scopes), level=MsgLevel.DEBUG)

    def with_scope(self, scopename, thunk):
        """Run thunk (0-argument function) with the scope stack augmented with an inner scope.
        Used to analyze lambda, listcomp et al. (The scope must still be present in self.scopes.)"""
        self.name_stack.append(scopename)
        inner_ns = self.get_current_namespace().get_name()
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

        if not len(self.name_stack):  # the top level is the current module
            return self.get_node('', self.module_name, None)

        namespace = '.'.join(self.name_stack[0:-1])
        name = self.name_stack[-1]
        return self.get_node(namespace, name, None)

    def get_value(self, name):
        """Get the value of name in the current scope. Return None if name is not set to a value."""

        # resolve "self"
        #
        # FIXME: we handle self by its literal name. If we want to change this,
        # need to capture the name of the first argument in visit_FunctionDef()
        # on the condition that self.current_class is not None.
        #
        current_class = self.get_current_class()
        if current_class is not None and name == 'self':
            self.msgprinter.message('name %s maps to %s' % (name, current_class), level=MsgLevel.INFO)
            name = current_class.name

        # get the innermost scope that has name **and where name has a value**
        def find_scope(name):
            for sc in reversed(self.scope_stack):
                if name in sc.defs and sc.defs[name] is not None:
                    return sc

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
        """Set the value of name in the current scope."""

        if name == 'self':
            self.msgprinter.message('WARNING: ignoring explicit assignment to special name "self" in %s' % (self.scope_stack[-1]), level=MsgLevel.WARNING)
            return

        # get the innermost scope that has name (should be the current scope unless name is a global)
        def find_scope(name):
            for sc in reversed(self.scope_stack):
                if name in sc.defs:
                    return sc

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
        if namespace in self.module_to_filename:
            # If the namespace is one of the modules being analyzed,
            # the the Node belongs to the correponding file.
            filename = self.module_to_filename[namespace]
        else:  # assume it's defined in the current file
            filename = self.filename

        n = Node(namespace, name, ast_node, filename)

        # Make the scope info accessible for the visit_*() methods
        # that only have an AST node.
        #
        # In Python 3, symtable and ast are completely separate, so symtable
        # doesn't see our copy of the AST, and symtable's copy of the AST
        # is not accessible from the outside.
        #
        # The visitor only gets an AST, but must be able to access the scope
        # information, so we mediate this by saving the full name of the namespace
        # where each AST node came from when it is get_node()d for the first time.
        #
        if ast_node is not None:
            self.ast_node_to_namespace[ast_node] = namespace
            self.msgprinter.message("Namespace for AST node %s (%s) recorded as '%s'" % (ast_node, name, namespace), level=MsgLevel.DEBUG)

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
        has to wait until the actual definition is found. However, a graph Node
        associated with an AST node must be created immediately, to track the
        uses of that function.

        This method re-associates the given graph node with a different
        AST node, which allows updating the context when the definition
        of a function or class is encountered."""
        graph_node.ast_node = ast_node
        if ast_node is not None:
            # Add also the new AST node to the reverse lookup.
            self.ast_node_to_namespace[ast_node] = graph_node.namespace
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
        if to_node == from_node:
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
