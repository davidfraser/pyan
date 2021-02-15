from glob import glob
import logging
import os

import pytest

from pyan.analyzer import CallGraphVisitor


@pytest.fixture
def callgraph():
    filenames = glob(os.path.join(os.path.dirname(__file__), "test_code/**/*.py"), recursive=True)
    v = CallGraphVisitor(filenames, logger=logging.getLogger())
    return v


def get_node(nodes, name):
    filtered_nodes = [node for node in nodes if node.get_name() == name]
    assert len(filtered_nodes) == 1, f"Node with name {name} should exist"
    return filtered_nodes[0]


def get_in_dict(node_dict, name):
    return node_dict[get_node(node_dict.keys(), name)]


def test_resolve_import_as(callgraph):
    imports = get_in_dict(callgraph.uses_edges, "test_code.submodule2")
    get_node(imports, "test_code.submodule1")
    assert len(imports) == 1, "only one effective import"

    imports = get_in_dict(callgraph.uses_edges, "test_code.submodule1")
    get_node(imports, "test_code.subpackage1.submodule1.A")
    get_node(imports, "test_code.subpackage1")


def test_import_relative(callgraph):
    imports = get_in_dict(callgraph.uses_edges, "test_code.subpackage1.submodule1")
    get_node(imports, "test_code.submodule2.test_2")


def test_resolve_use_in_class(callgraph):
    uses = get_in_dict(callgraph.uses_edges, "test_code.subpackage1.submodule1.A.__init__")
    get_node(uses, "test_code.submodule2.test_2")


def test_resolve_use_in_function(callgraph):
    uses = get_in_dict(callgraph.uses_edges, "test_code.submodule2.test_2")
    get_node(uses, "test_code.submodule1.test_func1")
    get_node(uses, "test_code.submodule1.test_func2")


def test_resolve_package_without___init__(callgraph):
    defines = get_in_dict(callgraph.defines_edges, "test_code.subpackage2.submodule_hidden1")
    get_node(defines, "test_code.subpackage2.submodule_hidden1.test_func1")


def test_resolve_package_with_known_root():
    dirname = os.path.dirname(__file__)
    filenames = glob(os.path.join(dirname, "test_code/**/*.py"), recursive=True)
    callgraph = CallGraphVisitor(filenames, logger=logging.getLogger(), root=dirname)
    dirname_base = os.path.basename(dirname)
    defines = get_in_dict(callgraph.defines_edges, f"{dirname_base}.test_code.subpackage2.submodule_hidden1")
    get_node(defines, f"{dirname_base}.test_code.subpackage2.submodule_hidden1.test_func1")
