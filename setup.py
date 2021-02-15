# -*- coding: utf-8 -*-
"""setuptools-based setup.py for pyan3.

Tested on Python 3.6.

Usage as usual with setuptools:
    python3 setup.py build
    python3 setup.py sdist
    python3 setup.py bdist_wheel --universal
    python3 setup.py install

For details, see
    http://setuptools.readthedocs.io/en/latest/setuptools.html#command-reference
or
    python3 setup.py --help
    python3 setup.py --help-commands
    python3 setup.py --help bdist_wheel  # or any command
"""

import ast
import os

from setuptools import setup

#########################################################
# General config
#########################################################

# Short description for package list on PyPI
#
SHORTDESC = "Offline call graph generator for Python 3"

# Long description for package homepage on PyPI
#
DESC = (
    "Generate approximate call graphs for Python programs.\n"
    "\n"
    "Pyan takes one or more Python source files, performs a "
    "(rather superficial) static analysis, and constructs a directed graph of "
    "the objects in the combined source, and how they define or "
    "use each other. The graph can be output for rendering by GraphViz or yEd."
)

#########################################################
# Init
#########################################################

# Extract __version__ from the package __init__.py
# (since it's not a good idea to actually run __init__.py during the
#  build process).
#
# https://stackoverflow.com/q/2058802/1959808
#
init_py_path = os.path.join("pyan", "__init__.py")
version = None
try:
    with open(init_py_path) as f:
        for line in f:
            if line.startswith("__version__"):
                module = ast.parse(line)
                expr = module.body[0]
                v = expr.value
                if type(v) is ast.Constant:
                    version = v.value
                elif type(v) is ast.Str:  # TODO: Python 3.8: remove ast.Str
                    version = v.s
                break
except FileNotFoundError:
    pass
if not version:
    raise RuntimeError(f"Version information not found in {init_py_path}")

#########################################################
# Call setup()
#########################################################

setup(
    name="pyan3",
    version=version,
    author="Juha Jeronen",
    author_email="juha.m.jeronen@gmail.com",
    url="https://github.com/Technologicat/pyan",
    description=SHORTDESC,
    long_description=DESC,
    license="GPL 2.0",
    # free-form text field;
    # https://stackoverflow.com/q/34994130/1959808
    platforms=["Linux"],
    # See
    #    https://pypi.python.org/pypi?%3Aaction=list_classifiers
    #
    # for the standard classifiers.
    #
    classifiers=[
        "Development Status :: 4 - Beta",
        "Environment :: Console",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU General Public License v2 (GPLv2)",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Topic :: Software Development",
    ],
    # See
    #    http://setuptools.readthedocs.io/en/latest/setuptools.html
    #
    setup_requires=["wheel"],
    install_requires=["jinja2"],
    provides=["pyan"],
    # keywords for PyPI (in case you upload your project)
    #
    # e.g. the keywords your project uses as topics on GitHub,
    # minus "python" (if there)
    #
    keywords=["call-graph", "static-code-analysis"],
    # Declare packages so that  python -m setup build  will copy .py files
    # (especially __init__.py).
    #
    # This **does not** automatically recurse into subpackages,
    # so they must also be declared.
    #
    packages=["pyan"],
    zip_safe=True,
    package_data={"pyan": ["callgraph.html"]},
    include_package_data=True,
    entry_points={
        "console_scripts": [
            "pyan3 = pyan.main:main",
        ]
    },
)
