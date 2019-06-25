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

import os
import ast
import sys
from setuptools import setup

#########################################################
# General config
#########################################################

# Name of the top-level package of the library.
#
# This is also the top level of its source tree, relative to the top-level project directory setup.py resides in.
#
libname = "pyan3"

# Short description for package list on PyPI
#
SHORTDESC = "Offline call graph generator for Python 3"

# Long description for package homepage on PyPI
#
DESC = """Generate approximate call graphs for Python programs.

Pyan takes one or more Python source files, performs a (rather superficial) static analysis, and constructs a directed graph of the objects in the combined source, and how they define or use each other. The graph can be output for rendering by GraphViz or yEd.
"""

# Set up data files for packaging.
#
# Directories (relative to the top-level directory where setup.py resides) in which to look for data files.
datadirs = ()

# File extensions to be considered as data files. (Literal, no wildcards.)
dataexts = (".py", ".ipynb", ".sh", ".lyx", ".tex", ".txt", ".pdf")

# Standard documentation to detect (and package if it exists).
#
standard_docs = ["README", "LICENSE", "TODO", "CHANGELOG", "AUTHORS"]  # just the basename without file extension
standard_doc_exts = [".md", ".rst", ".txt", ""]  # commonly .md for GitHub projects, but other projects may use .rst or .txt (or even blank).

#########################################################
# Init
#########################################################

# Gather user-defined data files
#
# http://stackoverflow.com/questions/13628979/setuptools-how-to-make-package-contain-extra-data-folder-and-all-folders-inside
#
datafiles = []
#getext = lambda filename: os.path.splitext(filename)[1]
#for datadir in datadirs:
#    datafiles.extend( [(root, [os.path.join(root, f) for f in files if getext(f) in dataexts])
#                       for root, dirs, files in os.walk(datadir)] )

# Add standard documentation (README et al.), if any, to data files
#
detected_docs = []
for docname in standard_docs:
    for ext in standard_doc_exts:
        filename = "".join((docname, ext))  # relative to the directory in which setup.py resides
        if os.path.isfile(filename):
            detected_docs.append(filename)
datafiles.append(('.', detected_docs))

# Extract __version__ from the package __init__.py
# (since it's not a good idea to actually run __init__.py during the build process).
#
# http://stackoverflow.com/questions/2058802/how-can-i-get-the-version-defined-in-setup-py-setuptools-in-my-package
#
init_py_path = os.path.join('pyan', '__init__.py')
version = '0.0.unknown'
try:
    with open(init_py_path) as f:
        for line in f:
            if line.startswith('__version__'):
                version = ast.parse(line).body[0].value.s
                break
        else:
            print("WARNING: Version information not found in '%s', using placeholder '%s'" % (init_py_path, version), file=sys.stderr)
except FileNotFoundError:
    print("WARNING: Could not find file '%s', using placeholder version information '%s'" % (init_py_path, version), file=sys.stderr)

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

    # free-form text field; http://stackoverflow.com/questions/34994130/what-platforms-argument-to-setup-in-setup-py-does
    platforms=["Linux"],

    # See
    #    https://pypi.python.org/pypi?%3Aaction=list_classifiers
    #
    # for the standard classifiers.
    #
    classifiers=["Development Status :: 4 - Beta",
                 "Environment :: Console",
                 "Intended Audience :: Developers",
                 "License :: OSI Approved :: GNU General Public License v2 (GPLv2)",
                 "Operating System :: POSIX :: Linux",
                 "Programming Language :: Python",
                 "Programming Language :: Python :: 3",
                 "Programming Language :: Python :: 3.6",
                 "Topic :: Software Development"
                 ],

    # See
    #    http://setuptools.readthedocs.io/en/latest/setuptools.html
    #
    setup_requires=[],
    install_requires=[],
    provides=["pyan"],

    # keywords for PyPI (in case you upload your project)
    #
    # e.g. the keywords your project uses as topics on GitHub, minus "python" (if there)
    #
    keywords=["call-graph", "static-code-analysis"],

    # Declare packages so that  python -m setup build  will copy .py files (especially __init__.py).
    #
    # This **does not** automatically recurse into subpackages, so they must also be declared.
    #
    packages=["pyan"],

    scripts=["pyan3"],

    zip_safe=True,

    # Custom data files not inside a Python package
    data_files=datafiles
)
