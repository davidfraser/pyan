#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""The AST visitor.

Created on Mon Nov 13 03:33:00 2017

Original code by Edmund Horner.
Python 3 port by Juha Jeronen.
"""

import logging
import ast
import symtable
from .common import format_alias, get_ast_node_name, \
                    sanitize_exprs, get_module_name, \
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

    def __init__(self, filenames, logger=None):
        self.logger = logger or logging.getLogger(__name__)

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

        self.class_base_ast_nodes = {}  # pass 1: class Node: list of AST nodes
        self.class_base_nodes     = {}  # pass 2: class Node: list of Node objects

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

    def resolve_base_classes(self):
        """Resolve base classes from AST nodes to Nodes.

        Run this between pass 1 and pass 2 to pick up inherited methods.
        Currently, this can parse ast.Names and ast.Attributes as bases.
        """
        self.logger.debug("Resolving base classes")
        assert len(self.scope_stack) == 0  # only allowed between passes
        for node in self.class_base_ast_nodes:
            self.class_base_nodes[node] = []
            for ast_node in self.class_base_ast_nodes[node]:
                # perform the lookup in the scope enclosing the class definition
                self.scope_stack.append(self.scopes[node.namespace])
                if isinstance(ast_node, ast.Name):
                    baseclass_node = self.get_value(ast_node.id)
                elif isinstance(ast_node, ast.Attribute):
                    _,baseclass_node = self.get_attribute(ast_node)  # don't care about obj, just grab attr
                else:  # give up
                    baseclass_node = None
                self.scope_stack.pop()

                if isinstance(baseclass_node, Node) and baseclass_node.namespace is not None:
                    self.class_base_nodes[node].append(baseclass_node)
        self.logger.debug("All base classes: %s" % self.class_base_nodes)

    def postprocess(self):
        """Finalize the analysis."""

        # Compared to the original Pyan, the ordering of expand_unknowns() and
        # contract_nonexistents() has been switched.
        #
        # It seems the original idea was to first convert any unresolved, but
        # specific, references to the form *.name, and then expand those to see
        # if they match anything else. However, this approach has the potential
        # to produce a lot of spurious uses edges (for unrelated functions with
        # a name that happens to match).
        #
        # Now that the analyzer is (very slightly) smarter about resolving
        # attributes and imports, we do it the other way around: we only expand
        # those references that could not be resolved to any known name, and
        # then remove any references pointing outside the analyzed file set.

        self.expand_unknowns()
        self.contract_nonexistents()
        self.cull_inherited()
        self.collapse_inner()

    ###########################################################################
    # visitor methods

    # Python docs:
    # https://docs.python.org/3/library/ast.html#abstract-grammar

    def visit_Module(self, node):
        self.logger.debug("Module")

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
        self.logger.debug("ClassDef %s" % (node.name))

        from_node = self.get_current_namespace()
        ns = from_node.get_name()
        to_node = self.get_node(ns, node.name, node)
        if self.add_defines_edge(from_node, to_node):
            self.logger.info("Def from %s to Class %s" % (from_node, to_node))

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

        self.class_base_ast_nodes[to_node] = []
        for b in node.bases:
            # gather info for resolution of inherited attributes in pass 2 (see get_attribute())
            self.class_base_ast_nodes[to_node].append(b)
            # mark uses from a derived class to its bases (via names appearing in a load context).
            self.visit(b)

        for stmt in node.body:
            self.visit(stmt)

        self.context_stack.pop()
        self.scope_stack.pop()
        self.name_stack.pop()
        self.class_stack.pop()

    def visit_FunctionDef(self, node):
        self.logger.debug("FunctionDef %s" % (node.name))

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
            self.logger.info("Def from %s to Function %s" % (from_node, to_node))

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
            self.logger.info('Method def: setting self name "%s" to %s' % (self_name, class_node))

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

        # Get the name representing "self", if applicable.
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

    def visit_Lambda(self, node):
        self.logger.debug("Lambda")
        def process():
            for d in node.args.defaults:
                self.visit(d)
            for d in node.args.kw_defaults:
                self.visit(d)
            self.visit(node.body)  # single expr
        self.with_scope("lambda", process)

    def visit_Import(self, node):
        self.logger.debug("Import %s" % [format_alias(x) for x in node.names])

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
            self.logger.debug("Use from %s to Import %s" % (from_node, to_node))
            if self.add_uses_edge(from_node, to_node):
                self.logger.info("New edge added for Use from %s to Import %s" % (from_node, to_node))

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
        self.logger.debug("ImportFrom: from %s import %s" % (node.module, [format_alias(x) for x in node.names]))

        tgt_name = node.module
        from_node = self.get_current_namespace()
        to_node = self.get_node('', tgt_name, node)  # module, in top-level namespace
        self.logger.debug("Use from %s to ImportFrom %s" % (from_node, to_node))
        if self.add_uses_edge(from_node, to_node):
            self.logger.info("New edge added for Use from %s to ImportFrom %s" % (from_node, to_node))

        if tgt_name in self.module_names:
            mod_name = self.module_names[tgt_name]
        else:
            mod_name = tgt_name

        for import_item in node.names:
            name = import_item.name
            new_name = import_item.asname if import_item.asname is not None else name
            tgt_id = self.get_node(mod_name, name, node)  # we imported the identifier name from the module mod_name
            self.set_value(new_name, tgt_id)
            self.logger.info("From setting name %s to %s" % (new_name, tgt_id))

#    # Edmund Horner's original post has info on what this fixed in Python 2.
#    # https://ejrh.wordpress.com/2012/01/31/call-graphs-in-python-part-2/
#    #
#    # Essentially, this should make '.'.join(...) see str.join.
#    #
#    # Python 3.4 does not have ast.Constant, but 3.6 does. Disabling for now.
#    # TODO: revisit this part after upgrading Python.
#    #
#    def visit_Constant(self, node):
#        self.logger.debug("Constant %s" % (node.value))
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

        self.logger.debug("Resolve %s.%s in context %s" % (get_ast_node_name(ast_node.value),
                                                           ast_node.attr, type(ast_node.ctx)))

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
                        self.logger.debug("Resolved to attr %s of %s" % (ast_node.attr, sc.defs[attr_name]))
                        return sc.defs[attr_name], ast_node.attr

            # It may happen that ast_node.value has no corresponding graph Node,
            # if this is a forward-reference, or a reference to a file
            # not in the analyzed set.
            #
            # In this case, return None for the object to let visit_Attribute()
            # add a wildcard reference to *.attr.
            #
            self.logger.debug("Unresolved, returning attr %s of unknown" % (ast_node.attr))
            return None, ast_node.attr
        else:
            # detect str.join() and similar (attributes of constant literals)
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

            # attribute of a function call. Detect cases like super().dostuff()
            elif isinstance(ast_node.value, ast.Call):
                obj_node = self.resolve_builtins(ast_node.value)

                # can't resolve result of general function call
                if not isinstance(obj_node, Node):
                    self.logger.debug("Unresolved function call as obj, returning attr %s of unknown" % (ast_node.attr))
                    return None, ast_node.attr
            else:
                # Get the Node object corresponding to node.value in the current ns.
                #
                # (Using the current ns here is correct; this case only gets
                #  triggered when there are no more levels of recursion,
                #  and the leftmost name always resides in the current ns.)
                obj_node = self.get_value(get_ast_node_name(ast_node.value))  # resolves "self" if needed

        self.logger.debug("Resolved to attr %s of %s" % (ast_node.attr, obj_node))
        return obj_node, ast_node.attr

    def get_attribute(self, ast_node):
        """Get value of an ast.Attribute.

        Supports inherited attributes. If the obj's own namespace has no match
        for attr, the ancestors of obj are also tried recursively until one of
        them matches or until all ancestors are exhausted.

        Return pair of Node objects (obj,attr), where each item can be None
        on lookup failure. (Object not known, or no Node value assigned
        to its attr.)
        """

        if not isinstance(ast_node.ctx, ast.Load):
            raise ValueError("Expected a load context, got %s" % (type(ast_node.ctx)))

        obj_node,attr_name = self.resolve_attribute(ast_node)

        if isinstance(obj_node, Node) and obj_node.namespace is not None:
            ns = obj_node.get_name()  # fully qualified namespace **of attr**

            # detect str.join() and similar (attributes of constant literals)
            #
            # Any attribute is considered valid for these special types,
            # but only in a load context. (set_attribute() does not have this
            # special handling, by design.)
            #
            if ns in ("Num", "Str"):  # TODO: other types?
                return obj_node, self.get_node(ns, attr_name, None)

            # look up attr_name in the given namespace, return Node or None
            def lookup(ns):
                if ns in self.scopes:
                    sc = self.scopes[ns]
                    if attr_name in sc.defs:
                        return sc.defs[attr_name]

            # first try directly in object's ns
            value_node = lookup(ns)
            if value_node is not None:
                return obj_node, value_node

            # next try ns of each ancestor (this works only in pass 2,
            # after self.class_base_nodes has been populated)
            #
            # TODO: MRO, multiple inheritance; Python uses C3 linearization
            #
            def lookup_in_bases_of(obj):
                if obj in self.class_base_nodes:  # has ancestors?
                    for base_node in self.class_base_nodes[obj]:
                        ns = base_node.get_name()
                        value_node = lookup(ns)
                        if value_node is not None:
                            return base_node, value_node
                        # recurse
                        b, v = lookup_in_bases_of(base_node)
                        if v is not None:
                            return b, v
                return None, None

            base_node, value_node = lookup_in_bases_of(obj_node)
            if value_node is not None:
                return base_node, value_node

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
        self.logger.debug("Attribute %s of %s in context %s" % (node.attr, objname, type(node.ctx)))

        # TODO: self.last_value is a hack. Handle names in store context (LHS)
        # in analyze_binding(), so that visit_Attribute() only needs to handle
        # the load context (i.e. detect uses of the name).
        #
        if isinstance(node.ctx, ast.Store):
            new_value = self.last_value
            if self.set_attribute(node, new_value):
                self.logger.info('setattr %s on %s to %s' % (node.attr, objname, new_value))

        elif isinstance(node.ctx, ast.Load):
            obj_node,attr_node = self.get_attribute(node)

            # Both object and attr known.
            if isinstance(attr_node, Node):
                self.logger.info('getattr %s on %s returns %s' % (node.attr, objname, attr_node))

                # add uses edge
                from_node = self.get_current_namespace()
                self.logger.debug("Use from %s to %s" % (from_node, attr_node))
                if self.add_uses_edge(from_node, attr_node):
                    self.logger.info("New edge added for Use from %s to %s" % (from_node, attr_node))

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
            # This sometimes creates silly nodes such as (when analyzing Pyan itself)
            # <Node pyan.analyzer.CallGraphVisitor.defines_edges.name.namespace>
            # but these are harmless, as they are considered undefined and
            # will not be visualized.
            #
            elif isinstance(obj_node, Node) and obj_node.namespace is not None:
                tgt_name = node.attr
                from_node = self.get_current_namespace()
                ns = obj_node.get_name()  # fully qualified namespace **of attr**
                to_node = self.get_node(ns, tgt_name, node)
                self.logger.debug("Use from %s to %s (target obj %s known but target attr %s not resolved; maybe fwd ref or unanalyzed import)" % (from_node, to_node, obj_node, node.attr))
                if self.add_uses_edge(from_node, to_node):
                    self.logger.info("New edge added for Use from %s to %s (target obj %s known but target attr %s not resolved; maybe fwd ref or unanalyzed import)" % (from_node, to_node, obj_node, node.attr))

                # remove resolved wildcard from current site to <Node *.attr>
                self.remove_wild(from_node, obj_node, node.attr)

                self.last_value = to_node

            # Object unknown, add uses edge to a wildcard by attr name.
            else:
                tgt_name = node.attr
                from_node = self.get_current_namespace()
                to_node = self.get_node(None, tgt_name, node)
                self.logger.debug("Use from %s to %s (target obj %s not resolved; maybe fwd ref, function argument, or unanalyzed import)" % (from_node, to_node, objname))
                if self.add_uses_edge(from_node, to_node):
                    self.logger.info("New edge added for Use from %s to %s (target obj %s not resolved; maybe fwd ref, function argument, or unanalyzed import)" % (from_node, to_node, objname))

                self.last_value = to_node

    # name access (node.ctx determines whether set (ast.Store) or get (ast.Load))
    def visit_Name(self, node):
        self.logger.debug("Name %s in context %s" % (node.id, type(node.ctx)))

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
                self.logger.debug("Use from %s to Name %s" % (from_node, to_node))
                if self.add_uses_edge(from_node, to_node):
                    self.logger.info("New edge added for Use from %s to Name %s" % (from_node, to_node))

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
            self.logger.debug("Assign (chained with %d outputs)" % (len(node.targets)))

        values = sanitize_exprs(node.value)  # values is the same for each set of targets
        for targets in node.targets:
            targets = sanitize_exprs(targets)
            self.logger.debug("Assign %s %s" % ([get_ast_node_name(x) for x in targets],
                                                [get_ast_node_name(x) for x in values]))
            self.analyze_binding(targets, values)

    def visit_AnnAssign(self, node):
        self.visit_Assign(self, node)  # TODO: alias for now; add the annotations to output in a future version?

    def visit_AugAssign(self, node):
        targets = sanitize_exprs(node.target)
        values = sanitize_exprs(node.value)  # values is the same for each set of targets

        self.logger.debug("AugAssign %s %s %s" % ([get_ast_node_name(x) for x in targets],
                                                  type(node.op),
                                                  [get_ast_node_name(x) for x in values]))

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
        self.logger.debug("For-loop")

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
        self.logger.debug("ListComp")
        def process():
            self.visit(node.elt)
            self.analyze_generators(node.generators)
        self.with_scope("listcomp", process)

    def visit_SetComp(self, node):
        self.logger.debug("SetComp")
        def process():
            self.visit(node.elt)
            self.analyze_generators(node.generators)
        self.with_scope("setcomp", process)

    def visit_DictComp(self, node):
        self.logger.debug("DictComp")
        def process():
            self.visit(node.key)
            self.visit(node.value)
            self.analyze_generators(node.generators)
        self.with_scope("dictcomp", process)

    def visit_GeneratorExp(self, node):
        self.logger.debug("GeneratorExp")
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

    def resolve_builtins(self, ast_node):
        """Resolve those calls to built-in functions whose return values
        can be determined in a simple manner.

        Currently, this supports only super() in a very rudimentary manner.
        This works only in pass 2."""
        if not isinstance(ast_node, ast.Call):
            raise TypeError("Expected ast.Call; got %s" % (type(ast_node)))

        func_ast_node = ast_node.func  # expr
        if isinstance(func_ast_node, ast.Name):
            funcname = func_ast_node.id
            if funcname == "super":
                class_node = self.get_current_class()
                self.logger.debug("Resolving super() of %s" % (class_node))
                if class_node in self.class_base_nodes:
                    base_nodes = self.class_base_nodes[class_node]
                    if len(base_nodes):
                        # TODO: MRO, multiple inheritance; Python uses C3 linearization
                        # (for now, we assume the super() call goes into the first base)
                        result = base_nodes[0]
                        self.logger.debug("super of %s is %s" % (class_node, result))
                        return result
            # add other funcnames here if needed

    def visit_Call(self, node):
        self.logger.debug("Call %s" % (get_ast_node_name(node.func)))

        # visit args to detect uses
        for arg in node.args:
            self.visit(arg)
        for kw in node.keywords:
            self.visit(kw.value)

        # see if we can predict the result
        result_node = self.resolve_builtins(node)
        if isinstance(result_node, Node):
            self.last_value = result_node
        else:  # generic function call
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

        self.logger.debug("Scopes now: %s" % (self.scopes))

    def with_scope(self, scopename, thunk):
        """Run thunk (0-argument function) with the scope stack augmented with an inner scope.
        Used to analyze lambda, listcomp et al. (The scope must still be present in self.scopes.)"""

        # The inner scopes pollute the graph too much; we will need to collapse
        # them in postprocessing. However, we must use them here to follow
        # the Python 3 scoping rules correctly.

        self.name_stack.append(scopename)
        inner_ns = self.get_current_namespace().get_name()
        if inner_ns not in self.scopes:
            raise ValueError("Unknown scope '%s'" % (inner_ns))
        self.scope_stack.append(self.scopes[inner_ns])
        self.context_stack.append(scopename)
        thunk()
        self.context_stack.pop()
        self.scope_stack.pop()
        self.name_stack.pop()

        # Add a defines edge, which will mark the inner scope as defined,
        # allowing any uses to other objects from inside the lambda/listcomp/etc.
        # body to be visualized.
        #
        # All inner scopes of the same scopename (lambda, listcomp, ...) in the
        # current ns will be grouped into a single node, as they have no name.
        # We create a namespace-like node that has no associated AST node,
        # as it does not represent any unique AST node.
        from_node = self.get_current_namespace()
        ns = from_node.get_name()
        to_node = self.get_node(ns, scopename, None)
        if self.add_defines_edge(from_node, to_node):
            self.logger.info("Def from %s to %s %s" % (from_node, scopename, to_node))
        self.last_value = to_node  # Make this inner scope node assignable to track its uses.


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
                self.logger.info('Get %s in %s, found in %s, value %s' % (name, self.scope_stack[-1], sc, value))
                return value
            else:
                # TODO: should always be a Node or None
                self.logger.debug('Get %s in %s, found in %s: value %s is not a Node' % (name, self.scope_stack[-1], sc, value))
        else:
            self.logger.debug('Get %s in %s: no Node value (or name not in scope)' % (name, self.scope_stack[-1]))

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
                self.logger.info('Set %s in %s to %s' % (name, sc, value))
            else:
                # TODO: should always be a Node or None
                self.logger.debug('Set %s in %s: value %s is not a Node' % (name, sc, value))
        else:
            self.logger.debug('Set: name %s not in scope' % (name))

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

        This needs both to_node and name because in case of a bound name
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
        #     (Actually, after commit e3c32b782a89b9eb225ef36d8557ebf172ff4ba5,
        #      this example is bad; sy.simplify will be recognized as an
        #      unknown attr of a known object, so no wildcard is generated.)
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
        # TODO: do we need to change our opinion now that also recursive calls are visualized?
        #
        if to_node == from_node:
            return

        matching_wilds = [n for n in self.uses_edges[from_node] if n.namespace is None and n.name == name]
        assert len(matching_wilds) < 2  # the set can have only one wild of matching name
        if len(matching_wilds):
            wild_node = matching_wilds[0]
            self.logger.info("Use from %s to %s resolves %s; removing wildcard" % (from_node, to_node, wild_node))
            self.uses_edges[from_node].remove(wild_node)

    ###########################################################################
    # Postprocessing

    def contract_nonexistents(self):
        """For all use edges to non-existent (i.e. not defined nodes) X.name, replace with edge to *.name."""

        new_uses_edges = []
        removed_uses_edges = []
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                if n2.namespace is not None and not n2.defined:
                    n3 = self.get_node(None, n2.name, n2.ast_node)
                    n3.defined = False
                    new_uses_edges.append((n, n3))
                    removed_uses_edges.append((n, n2))
                    self.logger.info("Contracting non-existent from %s to %s as %s" % (n, n2, n3))

        for from_node, to_node in new_uses_edges:
            self.add_uses_edge(from_node, to_node)

        for from_node, to_node in removed_uses_edges:
            self.uses_edges[from_node].remove(to_node)

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
            self.logger.info("Expanding unknowns: new defines edge from %s to %s" % (from_node, to_node))

        new_uses_edges = []
        for n in self.uses_edges:
            for n2 in self.uses_edges[n]:
                if n2.namespace is None:
                    for n3 in self.nodes[n2.name]:
                        if n3.namespace is not None:
                            new_uses_edges.append((n, n3))

        for from_node, to_node in new_uses_edges:
            self.add_uses_edge(from_node, to_node)
            self.logger.info("Expanding unknowns: new uses edge from %s to %s" % (from_node, to_node))

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
#                        if pn3 in self.uses_edges and pn2 in self.uses_edges[pn3]:  # remove the second edge W to Y.name (TODO: add an option to choose this)
                            inherited = True

                if inherited and n in self.uses_edges:
                    removed_uses_edges.append((n, n2))
                    self.logger.info("Removing inherited edge from %s to %s" % (n, n2))

        for from_node, to_node in removed_uses_edges:
            self.uses_edges[from_node].remove(to_node)

    def collapse_inner(self):
        """Combine lambda and comprehension Nodes with their parent Nodes to reduce visual noise.
        Also mark those original nodes as undefined, so that they won't be visualized."""

        # Lambdas and comprehensions do not define any names in the enclosing
        # scope, so we only need to treat the uses edges.

        # TODO: currently we handle outgoing uses edges only.
        #
        # What about incoming uses edges? E.g. consider a lambda that is saved
        # in an instance variable, then used elsewhere. How do we want the
        # graph to look like in that case?

        for name in self.nodes:
            if name in ('lambda', 'listcomp', 'setcomp', 'dictcomp', 'genexpr'):
                for n in self.nodes[name]:
                    nsp,p = n.namespace.rsplit('.', 1)  # parent
                    pn = self.get_node(nsp, p, None)
                    for n2 in self.uses_edges[n]:  # outgoing uses edges
                        self.logger.info("Collapsing inner from %s to %s, uses %s" % (n, pn, n2))
                        self.add_uses_edge(pn, n2)
                    n.defined = False
