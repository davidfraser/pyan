#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""The AST visitor."""

import logging
import ast
import symtable

from .node import Node, Flavor
from .anutils import tail, get_module_name, format_alias, \
                     get_ast_node_name, sanitize_exprs, \
                     resolve_method_resolution_order, \
                     Scope, ExecuteInInnerScope, UnresolvedSuperCallError

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
        self.class_base_nodes     = {}  # pass 2: class Node: list of Node objects (local bases, no recursion)
        self.mro                  = {}  # pass 2: class Node: list of Node objects in Python's MRO order

        # current context for analysis
        self.module_name = None
        self.filename = None
        self.name_stack  = []  # for building namespace name, node naming
        self.scope_stack = []  # the Scope objects currently in scope
        self.class_stack = []  # Nodes for class definitions currently in scope
        self.context_stack = []  # for detecting which FunctionDefs are methods
        self.last_value  = None

        # Analyze.
        self.process()

    def process(self):
        """Analyze the set of files, twice so that any forward-references are picked up."""
        for pas in range(2):
            for filename in self.filenames:
                self.logger.info("========== pass %d, file '%s' ==========" % (pas+1, filename))
                self.process_one(filename)
            if pas == 0:
                self.resolve_base_classes()  # must be done only after all files seen
        self.postprocess()

    def process_one(self, filename):
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
        for node in self.class_base_ast_nodes:  # Node: list of AST nodes
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

        self.logger.debug("All base classes (non-recursive, local level only): %s" % self.class_base_nodes)

        self.logger.debug("Resolving method resolution order (MRO) for all analyzed classes")
        self.mro = resolve_method_resolution_order(self.class_base_nodes, self.logger)
        self.logger.debug("Method resolution order (MRO) for all analyzed classes: %s" % self.mro)

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

    # In visit_*(), the "node" argument refers to an AST node.

    # Python docs:
    # https://docs.python.org/3/library/ast.html#abstract-grammar

    def visit_Module(self, node):
        self.logger.debug("Module")

        # Modules live in the top-level namespace, ''.
        module_node = self.get_node('', self.module_name, node, flavor=Flavor.MODULE)
        self.associate_node(module_node, node, filename=self.filename)

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

        from_node = self.get_node_of_current_namespace()
        ns = from_node.get_name()
        to_node = self.get_node(ns, node.name, node, flavor=Flavor.CLASS)
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
        inner_ns = self.get_node_of_current_namespace().get_name()
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

        # To begin with:
        #
        # - Analyze decorators. They belong to the surrounding scope,
        #   so we must analyze them before entering the function scope.
        #
        # - Determine whether this definition is for a function, an (instance)
        #   method, a static method or a class method.
        #
        # - Grab the name representing "self", if this is either an instance
        #   method or a class method. (For a class method, it represents cls,
        #   but Pyan only cares about types, not instances.)
        #
        self_name,flavor = self.analyze_functiondef(node)

        # Now we can create the Node.
        #
        from_node = self.get_node_of_current_namespace()
        ns = from_node.get_name()
        to_node = self.get_node(ns, node.name, node, flavor=flavor)
        if self.add_defines_edge(from_node, to_node):
            self.logger.info("Def from %s to Function %s" % (from_node, to_node))

        # Same remarks as for ClassDef above.
        #
        self.associate_node(to_node, node, self.filename)
        self.set_value(node.name, to_node)

        # Enter the function scope
        #
        self.name_stack.append(node.name)
        inner_ns = self.get_node_of_current_namespace().get_name()
        self.scope_stack.append(self.scopes[inner_ns])
        self.context_stack.append("FunctionDef %s" % (node.name))

        # Capture which names correspond to function args.
        #
        # In the function scope, set them to a nonsense Node,
        # to prevent leakage of identifiers of matching name
        # from the enclosing scope (due to the local value being None
        # until we set it to this nonsense Node).
        #
        # As the name of the nonsense node, we can use any string that
        # is not a valid Python identifier.
        #
        # It has no sensible flavor, so we leave its flavor unspecified.
        #
        sc = self.scopes[inner_ns]
        nonsense_node = self.get_node(inner_ns, '^^^argument^^^', None)
        all_args = node.args  # args, vararg (*args), kwonlyargs, kwarg (**kwargs)
        for a in all_args.args:  # positional
            sc.defs[a.arg] = nonsense_node
        if all_args.vararg is not None:  # *args if present
            sc.defs[all_args.vararg] = nonsense_node
        for a in all_args.kwonlyargs:
            sc.defs[a.arg] = nonsense_node
        if all_args.kwarg is not None:  # **kwargs if present
            sc.defs[all_args.kwarg] = nonsense_node

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

        # Exit the function scope
        #
        self.context_stack.pop()
        self.scope_stack.pop()
        self.name_stack.pop()

    def visit_AsyncFunctionDef(self, node):
        self.visit_FunctionDef(node)  # TODO: alias for now; tag async functions in output in a future version?

    def visit_Lambda(self, node):
        self.logger.debug("Lambda")
        with ExecuteInInnerScope(self, "lambda"):
            for d in node.args.defaults:
                self.visit(d)
            for d in node.args.kw_defaults:
                self.visit(d)
            self.visit(node.body)  # single expr

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
            # where it is being imported to, i.e. the **user**
            from_node = self.get_node_of_current_namespace()
            # the thing **being used** (under the asname, if any)
            to_node  = self.get_node('', tgt_name, node, flavor=Flavor.IMPORTEDITEM)

            is_new_edge = self.add_uses_edge(from_node, to_node)

            # bind asname in the current namespace to the imported module
            #
            # conversion: possible short name -> fully qualified name
            # (when analyzing a set of files in the same directory)
            if src_name in self.module_names:
                mod_name = self.module_names[src_name]
            else:
                mod_name = src_name
            tgt_module = self.get_node('', mod_name, node, flavor=Flavor.MODULE)
            # XXX: if there is no asname, it may happen that mod_name == tgt_name,
            # in which case these will be the same Node. They are semantically
            # distinct (Python name at receiving end, vs. module), but currently
            # Pyan has no way of retaining that information.
            if to_node is tgt_module:
                to_node.flavor = Flavor.MODULE
            self.set_value(tgt_name, tgt_module)

            # must do this after possibly munging flavor to avoid confusing
            # the user reading the log
            self.logger.debug("Use from %s to Import %s" % (from_node, to_node))
            if is_new_edge:
                self.logger.info("New edge added for Use from %s to Import %s" % (from_node, to_node))

    def visit_ImportFrom(self, node):
        self.logger.debug("ImportFrom: from %s import %s" % (node.module, [format_alias(x) for x in node.names]))

        tgt_name = node.module
        from_node = self.get_node_of_current_namespace()
        to_node = self.get_node('', tgt_name, node, flavor=Flavor.MODULE)  # module, in top-level namespace
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
            # we imported the identifier name from the module mod_name
            tgt_id = self.get_node(mod_name, name, node, flavor=Flavor.IMPORTEDITEM)
            self.set_value(new_name, tgt_id)
            self.logger.info("From setting name %s to %s" % (new_name, tgt_id))

#    # Edmund Horner's original post has info on what this fixed in Python 2.
#    # https://ejrh.wordpress.com/2012/01/31/call-graphs-in-python-part-2/
#    #
#    # Essentially, this should make '.'.join(...) see str.join.
#    # Pyan3 currently handles that in resolve_attribute() and get_attribute().
#    #
#    # Python 3.4 does not have ast.Constant, but 3.6 does. Disabling for now.
#    # TODO: revisit this part after upgrading Python.
#    #
#    def visit_Constant(self, node):
#        self.logger.debug("Constant %s" % (node.value))
#        t = type(node.value)
#        tn = t.__name__
#        self.last_value = self.get_node('', tn, node)

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
            try:
                if self.set_attribute(node, new_value):
                    self.logger.info('setattr %s on %s to %s' % (node.attr, objname, new_value))
            except UnresolvedSuperCallError:
                # Trying to set something belonging to an unresolved super()
                # of something; just ignore this attempt to setattr.
                return

        elif isinstance(node.ctx, ast.Load):
            try:
                obj_node,attr_node = self.get_attribute(node)
            except UnresolvedSuperCallError:
                # Avoid adding a wildcard if the lookup failed due to an
                # unresolved super() in the attribute chain.
                return

            # Both object and attr known.
            if isinstance(attr_node, Node):
                self.logger.info('getattr %s on %s returns %s' % (node.attr, objname, attr_node))

                # add uses edge
                from_node = self.get_node_of_current_namespace()
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
                from_node = self.get_node_of_current_namespace()
                ns = obj_node.get_name()  # fully qualified namespace **of attr**
                to_node = self.get_node(ns, tgt_name, node, flavor=Flavor.ATTRIBUTE)
                self.logger.debug("Use from %s to %s (target obj %s known but target attr %s not resolved; maybe fwd ref or unanalyzed import)" % (from_node, to_node, obj_node, node.attr))
                if self.add_uses_edge(from_node, to_node):
                    self.logger.info("New edge added for Use from %s to %s (target obj %s known but target attr %s not resolved; maybe fwd ref or unanalyzed import)" % (from_node, to_node, obj_node, node.attr))

                # remove resolved wildcard from current site to <Node *.attr>
                self.remove_wild(from_node, obj_node, node.attr)

                self.last_value = to_node

            # Object unknown, add uses edge to a wildcard by attr name.
            else:
                tgt_name = node.attr
                from_node = self.get_node_of_current_namespace()
                to_node = self.get_node(None, tgt_name, node, flavor=Flavor.UNKNOWN)
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
                    # namespace=None means we don't know the namespace yet
                    to_node = self.get_node(None, tgt_name, node, flavor=Flavor.UNKNOWN)

                from_node = self.get_node_of_current_namespace()
                self.logger.debug("Use from %s to Name %s" % (from_node, to_node))
                if self.add_uses_edge(from_node, to_node):
                    self.logger.info("New edge added for Use from %s to Name %s" % (from_node, to_node))

            self.last_value = to_node

    def visit_Assign(self, node):
        # - chaining assignments like "a = b = c" produces multiple targets
        # - tuple unpacking works as a separate mechanism on top of that (see analyze_binding())
        #
        if len(node.targets) > 1:
            self.logger.debug("Assign (chained with %d outputs)" % (len(node.targets)))

        # TODO: support lists, dicts, sets (so that we can recognize calls to their methods)
        # TODO: begin with supporting empty lists, dicts, sets
        # TODO: need to be more careful in sanitizing; currently destroys a bare list

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
        with ExecuteInInnerScope(self, "listcomp"):
            self.visit(node.elt)
            self.analyze_generators(node.generators)

    def visit_SetComp(self, node):
        self.logger.debug("SetComp")
        with ExecuteInInnerScope(self, "setcomp"):
            self.visit(node.elt)
            self.analyze_generators(node.generators)

    def visit_DictComp(self, node):
        self.logger.debug("DictComp")
        with ExecuteInInnerScope(self, "dictcomp"):
            self.visit(node.key)
            self.visit(node.value)
            self.analyze_generators(node.generators)

    def visit_GeneratorExp(self, node):
        self.logger.debug("GeneratorExp")
        with ExecuteInInnerScope(self, "genexpr"):
            self.visit(node.elt)
            self.analyze_generators(node.generators)

    def visit_Call(self, node):
        self.logger.debug("Call %s" % (get_ast_node_name(node.func)))

        # visit args to detect uses
        for arg in node.args:
            self.visit(arg)
        for kw in node.keywords:
            self.visit(kw.value)

        # see if we can predict the result
        try:
            result_node = self.resolve_builtins(node)
        except UnresolvedSuperCallError:
            result_node = None

        if isinstance(result_node, Node):  # resolved result
            self.last_value = result_node

            from_node = self.get_node_of_current_namespace()
            to_node = result_node
            self.logger.debug("Use from %s to %s (via resolved call to built-ins)" % (from_node, to_node))
            if self.add_uses_edge(from_node, to_node):
                self.logger.info("New edge added for Use from %s to %s (via resolved call to built-ins)" % (from_node, to_node))

        else:  # generic function call
            # Visit the function name part last, so that inside a binding form,
            # it will be left standing as self.last_value.
            self.visit(node.func)

            # If self.last_value matches a known class i.e. the call was of the
            # form MyClass(), add a uses edge to MyClass.__init__().
            #
            # We need to do this manually, because there is no text "__init__"
            # at the call site.
            #
            # In this lookup to self.class_base_ast_nodes we don't care about
            # the AST nodes; the keys just conveniently happen to be the Nodes
            # of known classes.
            #
            if self.last_value in self.class_base_ast_nodes:
                from_node = self.get_node_of_current_namespace()
                class_node = self.last_value
                to_node = self.get_node(class_node.get_name(), '__init__', None, flavor=Flavor.METHOD)
                self.logger.debug("Use from %s to %s (call creates an instance)" % (from_node, to_node))
                if self.add_uses_edge(from_node, to_node):
                    self.logger.info("New edge added for Use from %s to %s (call creates an instance)" % (from_node, to_node))

    def visit_With(self, node):
        self.logger.debug("With (context manager)")

        def add_uses_enter_exit_of(graph_node):
            # add uses edges to __enter__ and __exit__ methods of given Node
            if isinstance(graph_node, Node):
                from_node = self.get_node_of_current_namespace()
                withed_obj_node = graph_node

                self.logger.debug("Use from %s to With %s" % (from_node, withed_obj_node))
                for methodname in ('__enter__', '__exit__'):
                    to_node = self.get_node(withed_obj_node.get_name(), methodname, None, flavor=Flavor.METHOD)
                    if self.add_uses_edge(from_node, to_node):
                        self.logger.info("New edge added for Use from %s to %s" % (from_node, to_node))

        for withitem in node.items:
            expr = withitem.context_expr
            vars = withitem.optional_vars

            # XXX: we currently visit expr twice (again in analyze_binding()) if vars is not None
            self.last_value = None
            self.visit(expr)
            add_uses_enter_exit_of(self.last_value)
            self.last_value = None

            if vars is not None:
                # bind optional_vars
                #
                # TODO: For now, we support only the following (most common) case:
                #  - only one binding target, vars is ast.Name
                #    (not ast.Tuple or something else)
                #  - the variable will point to the object that was with'd
                #    (i.e. we assume the object's __enter__() method
                #     to finish with "return self")
                #
                if isinstance(vars, ast.Name):
                    self.analyze_binding(sanitize_exprs(vars), sanitize_exprs(expr))
                else:
                    self.visit(vars)  # just capture any uses on the With line itself

        for stmt in node.body:
            self.visit(stmt)

    ###########################################################################
    # Analysis helpers

    def analyze_functiondef(self, ast_node):
        """Analyze a function definition.

        Visit decorators, and if this is a method definition, capture the name
        of the first positional argument to denote "self", like Python does.

        Return (self_name, flavor), where self_name the name representing self,
        or None if not applicable; and flavor is a Flavor, specifically one of
        FUNCTION, METHOD, STATICMETHOD or CLASSMETHOD."""

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

        # Analyze flavor
        in_class_ns = self.context_stack[-1].startswith("ClassDef")
        if not in_class_ns:
            flavor = Flavor.FUNCTION
        else:
            if "staticmethod" in deco_names:
                flavor = Flavor.STATICMETHOD
            elif "classmethod" in deco_names:
                flavor = Flavor.CLASSMETHOD
            else:  # instance method
                flavor = Flavor.METHOD

        # Get the name representing "self", if applicable.
        #
        # - ignore static methods
        # - ignore functions defined inside methods (this new FunctionDef
        #   must be directly in a class namespace)
        #
        if flavor in (Flavor.METHOD, Flavor.CLASSMETHOD):
            # We can treat instance methods and class methods the same,
            # since Pyan is only interested in object types, not instances.
            all_args = ast_node.args  # args, vararg (*args), kwonlyargs, kwarg (**kwargs)
            posargs = all_args.args
            if len(posargs):
                self_name = posargs[0].arg
                return self_name, flavor

        return None, flavor

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
            captured_values = []
            for value in values:
                self.visit(value)  # RHS -> set self.last_value
                captured_values.append(self.last_value)
                self.last_value = None
            for tgt,val in zip(targets,captured_values):
                self.last_value = val
                self.visit(tgt)    # LHS, name in a store context
            self.last_value = None
        else:  # FIXME: for now, do the wrong thing in the non-trivial case
            # old code, no tuple unpacking support
            for value in values:
                self.visit(value)  # set self.last_value to **something** on the RHS and hope for the best
            for tgt in targets:    # LHS, name in a store context
                self.visit(tgt)
            self.last_value = None

    def analyze_generators(self, generators):
        """Analyze the generators in a comprehension form.

        Analyzes the binding part, and visits the "if" expressions (if any).

        generators: an iterable of ast.comprehension objects
        """

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

        Currently, this supports:

          - str(obj), repr(obj) --> obj.__str__, obj.__repr__

          - super() (any arguments ignored), which works only in pass 2,
            because the MRO is determined between passes.

        May raise UnresolvedSuperCallError, if the call is to super(),
        but the result cannot be (currently) determined (usually because either
        pass 1, or some relevant source file is not in the analyzed set).

        Returns the Node the call resolves to, or None if not determined.
        """
        if not isinstance(ast_node, ast.Call):
            raise TypeError("Expected ast.Call; got %s" % (type(ast_node)))

        func_ast_node = ast_node.func  # expr
        if isinstance(func_ast_node, ast.Name):
            funcname = func_ast_node.id
            if funcname == "super":
                class_node = self.get_current_class()
                self.logger.debug("Resolving super() of %s" % (class_node))
                if class_node in self.mro:
                    # Our super() class is the next one in the MRO.
                    #
                    # Note that we consider only the **static type** of the
                    # class itself. The later elements of the MRO - important
                    # for resolving chained super() calls in a dynamic context,
                    # where the dynamic type of the calling object is different
                    # from the static type of the class where the super() call
                    # site is - are never used by Pyan for resolving super().
                    #
                    # This is a limitation of pure lexical scope based static
                    # code analysis.
                    #
                    if len(self.mro[class_node]) > 1:
                        result = self.mro[class_node][1]
                        self.logger.debug("super of %s is %s" % (class_node, result))
                        return result
                    else:
                        msg = "super called for %s, but no known bases" % (class_node)
                        self.logger.info(msg)
                        raise UnresolvedSuperCallError(msg)
                else:
                    msg = "super called for %s, but MRO not determined for it (maybe still in pass 1?)" % (class_node)
                    self.logger.info(msg)
                    raise UnresolvedSuperCallError(msg)

            if funcname in ("str", "repr"):
                if len(ast_node.args) == 1:  # these take only one argument
                    obj_astnode = ast_node.args[0]
                    if isinstance(obj_astnode, (ast.Name, ast.Attribute)):
                        self.logger.debug("Resolving %s() of %s" % (funcname, get_ast_node_name(obj_astnode)))
                        attrname = "__%s__" % (funcname)
                        # build a temporary ast.Attribute AST node so that we can use get_attribute()
                        tmp_astnode = ast.Attribute(value=obj_astnode, attr=attrname, ctx=obj_astnode.ctx)
                        obj_node, attr_node = self.get_attribute(tmp_astnode)
                        self.logger.debug("Resolve %s() of %s: returning attr node %s" % (funcname, get_ast_node_name(obj_astnode), attr_node))
                        return attr_node

            # add implementations for other built-in funcnames here if needed

    def resolve_attribute(self, ast_node):
        """Resolve an ast.Attribute.

        Nested attributes (a.b.c) are automatically handled by recursion.

        Return (obj,attrname), where obj is a Node (or None on lookup failure),
        and attrname is the attribute name.

        May pass through UnresolvedSuperCallError, if the attribute resolution
        failed specifically due to an unresolved super() call.
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
                #
                # The CLASS flavor is the best match, as these constants
                # are object types.
                #
                obj_node = self.get_node('', tn, None, flavor=Flavor.CLASS)

            # attribute of a function call. Detect cases like super().dostuff()
            elif isinstance(ast_node.value, ast.Call):
                # Note that resolve_builtins() will signal an unresolved
                # super() by an exception, which we just pass through here.
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

    def get_current_class(self):
        """Return the node representing the current class, or None if not inside a class definition."""
        return self.class_stack[-1] if len(self.class_stack) else None

    def get_node_of_current_namespace(self):
        """Return the unique node representing the current namespace,
        based on self.name_stack.

        For a Node n representing a namespace:
          - n.namespace = fully qualified name of the parent namespace
                          (empty string if at top level)
          - n.name      = name of this namespace
          - no associated AST node.
        """
        assert len(self.name_stack)  # name_stack should never be empty (always at least module name)

        namespace = '.'.join(self.name_stack[0:-1])
        name = self.name_stack[-1]
        return self.get_node(namespace, name, None, flavor=Flavor.NAMESPACE)

    ###########################################################################
    # Value getter and setter

    def get_value(self, name):
        """Get the value of name in the current scope. Return the Node, or None
        if name is not set to a value."""

        # get the innermost scope that has name **and where name has a value**
        def find_scope(name):
            for sc in reversed(self.scope_stack):
                if name in sc.defs and sc.defs[name] is not None:
                    return sc

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
    # Attribute getter and setter

    def get_attribute(self, ast_node):
        """Get value of an ast.Attribute.

        Supports inherited attributes. If the obj's own namespace has no match
        for attr, the ancestors of obj are also tried, following the MRO based
        on the static type of the object, until one of them matches or until
        all ancestors are exhausted.

        Return pair of Node objects (obj,attr), where each item can be None
        on lookup failure. (Object not known, or no Node value assigned
        to its attr.)

        May pass through UnresolvedSuperCallError.
        """

        if not isinstance(ast_node, ast.Attribute):
            raise TypeError("Expected ast.Attribute; got %s" % (type(ast_node)))
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
                return obj_node, self.get_node(ns, attr_name, None, flavor=Flavor.ATTRIBUTE)

            # look up attr_name in the given namespace, return Node or None
            def lookup(ns):
                if ns in self.scopes:
                    sc = self.scopes[ns]
                    if attr_name in sc.defs:
                        return sc.defs[attr_name]

            # first try directly in object's ns (this works already in pass 1)
            value_node = lookup(ns)
            if value_node is not None:
                return obj_node, value_node

            # next try ns of each ancestor (this works only in pass 2,
            # after self.mro has been populated)
            #
            if obj_node in self.mro:
                for base_node in tail(self.mro[obj_node]):  # the first element is always obj itself
                    ns = base_node.get_name()
                    value_node = lookup(ns)
                    if value_node is not None:
                        break
                else:
                    return None, None  # not found
                return base_node, value_node  # as obj, return the base class in which attr was found

        return obj_node, None  # here obj_node is either None or unknown (namespace None)

    def set_attribute(self, ast_node, new_value):
        """Assign the Node provided as new_value into the attribute described
        by the AST node ast_node. Return True if assignment was done,
        False otherwise.

        May pass through UnresolvedSuperCallError.
        """

        if not isinstance(ast_node, ast.Attribute):
            raise TypeError("Expected ast.Attribute; got %s" % (type(ast_node)))
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

    ###########################################################################
    # Graph creation

    def get_node(self, namespace, name, ast_node=None, flavor=Flavor.UNSPECIFIED):
        """Return the unique node matching the namespace and name.
        Create a new node if one doesn't already exist.

        To associate the node with a syntax object in the analyzed source code,
        an AST node can be passed in. This only takes effect if a new Node
        is created.

        To associate an AST node to an existing graph node,
        see associate_node().

        Flavor describes the kind of object the node represents.
        See the node.Flavor enum for currently supported values.

        For existing nodes, flavor overwrites, if the given flavor is
        (strictly) more specific than the node's existing one.
        See node.Flavor.specificity().

        !!!
        In CallGraphVisitor, always use get_node() to create nodes, because it
        also sets some important auxiliary information. Do not call the Node
        constructor directly.
        !!!
        """

        if name in self.nodes:
            for n in self.nodes[name]:
                if n.namespace == namespace:
                    if Flavor.specificity(flavor) > Flavor.specificity(n.flavor):
                        n.flavor = flavor
                    return n

        # Try to figure out which source file this Node belongs to
        # (for annotated output).
        #
        # Other parts of the analyzer may change the filename later,
        # if a more authoritative source (e.g. a definition site) is found,
        # so the filenames should be trusted only after the analysis is
        # complete.
        #
        # TODO: this is tentative. Add in filename only when sure?
        # (E.g. in visit_ClassDef(), visit_FunctionDef())
        #
        if namespace in self.module_to_filename:
            # If the namespace is one of the modules being analyzed,
            # the the Node belongs to the correponding file.
            filename = self.module_to_filename[namespace]
        else:  # Assume the Node belongs to the current file.
            filename = self.filename

        n = Node(namespace, name, ast_node, filename, flavor)

        # Add to the list of nodes that have this short name.
        if name in self.nodes:
            self.nodes[name].append(n)
        else:
            self.nodes[name] = [n]

        return n

    def get_parent_node(self, graph_node):
        """Get the parent node of the given Node. (Used in postprocessing.)"""
        if '.' in graph_node.namespace:
            ns,name = graph_node.namespace.rsplit('.', 1)
        else:
            ns,name = '',graph_node.namespace
        return self.get_node(ns, name, None)

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
        to avoid a wildcard and the over-reaching expand_unknowns() in cases
        where they are not needed).

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

    def remove_uses_edge(self, from_node, to_node):
        """Remove a uses edge from the graph. (Used in postprocessing.)"""

        if from_node in self.uses_edges:
            u = self.uses_edges[from_node]
            if to_node in u:
                u.remove(to_node)

    def remove_wild(self, from_node, to_node, name):
        """Remove uses edge from from_node to wildcard *.name.

        This needs both to_node and name because in case of a bound name
        (e.g. attribute lookup) the name field of the *target value* does not
        necessarily match the formal name in the wildcard.

        Used for cleaning up forward-references once resolved.
        This prevents spurious edges due to expand_unknowns()."""

        if from_node not in self.uses_edges:  # no uses edges to remove
            return

        # Keep wildcard if the target is actually an unresolved argument
        # (see visit_FunctionDef())
        if to_node.get_name().find("^^^argument^^^") != -1:
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
            self.remove_uses_edge(from_node, wild_node)

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
            self.remove_uses_edge(from_node, to_node)

    def expand_unknowns(self):
        """For each unknown node *.name, replace all its incoming edges with edges to X.name for all possible Xs.

        Also mark all unknown nodes as not defined (so that they won't be visualized)."""

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
                        pn2 = self.get_parent_node(n2)
                        pn3 = self.get_parent_node(n3)
                        if pn2 in self.uses_edges and pn3 in self.uses_edges[pn2]:  # remove the first edge W to X.name
#                        if pn3 in self.uses_edges and pn2 in self.uses_edges[pn3]:  # remove the second edge W to Y.name (TODO: add an option to choose this)
                            inherited = True

                if inherited and n in self.uses_edges:
                    removed_uses_edges.append((n, n2))
                    self.logger.info("Removing inherited edge from %s to %s" % (n, n2))

        for from_node, to_node in removed_uses_edges:
            self.remove_uses_edge(from_node, to_node)

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
                    pn = self.get_parent_node(n)
                    if n in self.uses_edges:
                        for n2 in self.uses_edges[n]:  # outgoing uses edges
                            self.logger.info("Collapsing inner from %s to %s, uses %s" % (n, pn, n2))
                            self.add_uses_edge(pn, n2)
                    n.defined = False
