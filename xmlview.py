#!/usr/bin/env python


import os
import sys
import re
import time
import tempfile
from optparse import OptionParser
from glob import glob
import libxml2
import libxslt
import webbrowser
import os.path

STYLESHEET_XSL = 'stylesheet.xsl'
MERGE_XSL = 'merge.xsl'


def get_stylesheet_path(stylesheet_name):
    base_path = os.path.dirname(sys.argv[0])
    return os.path.join(base_path, stylesheet_name)
    

def tempname(suffix):
    f, n = tempfile.mkstemp(suffix)
    os.close(f)
    return n


xsl_cache = {}

def transform_doc(doc, stylesheet, params={}):
    if stylesheet not in xsl_cache:
        xsl_doc = libxml2.parseFile(stylesheet)
        xsl = libxslt.parseStylesheetDoc(xsl_doc)
        if xsl is None:
            raise Exception("Failed to parse XML file into a stylesheet: %s" % stylesheet)
        xsl_cache[stylesheet] = xsl
    result = xsl_cache[stylesheet].applyStylesheet(doc, params)
    return result


def process_file(filename):
    tempfile = tempname('.html')
    
    doc = libxml2.parseFile(filename)
    stylesheet = get_stylesheet_path(STYLESHEET_XSL)
    doc2 = transform_doc(doc, stylesheet, {'filename': "'%s'" % filename})
        
    f = file(tempfile, 'wt')
    f.write(str(doc2))
    f.close()
    
    webbrowser.open(tempfile)


def process_merge(filename1, filename2):
    tempfile = tempname('.xml')
    othertempfile = tempname('.xml')
    
    doc = libxml2.parseFile(filename2)
    f = file(othertempfile, 'wt')
    f.write(str(doc))
    f.close()
    
    doc = libxml2.parseFile(filename1)
    stylesheet = get_stylesheet_path(MERGE_XSL)
    doc2 = transform_doc(doc, stylesheet, {'other': "'%s'" % othertempfile.replace('\\', '/')})
        
    f = file(tempfile, 'wt')
    f.write(str(doc2))
    f.close()
    
    return tempfile


def process_diff(filename1, filename2):
    tempfile = tempname('.xml')
    
    doc = libxml2.parseFile(filename1)
    
    stylesheet = get_stylesheet_path(MERGE_XSL)
    doc2 = transform_doc(doc, stylesheet, {'other': "'%s'" % filename2.replace('\\', '/')})
        
    f = file(tempfile, 'wt')
    f.write(str(doc2))
    f.close()
    
    return tempfile


def main(argv=None):
    if argv is None:
        argv = sys.argv

    parser = OptionParser(usage="usage: %prog [options]")
    parser.add_option("-m", "--merge", default=False, action='store_true')
    parser.add_option("-d", "--diff", default=False, action='store_true')
    (options, args) = parser.parse_args(argv[1:])
    all_args = [b for a in args for b in glob(a)]

    if len(all_args) == 0:
        parser.error("You should probably specify a file")
    
    if (options.merge or options.diff) and len(all_args) != 2:
        parser.error("Need to specify exactly two files if merging")
    
    if options.merge:
        all_args = [process_merge(all_args[0], all_args[1])]
    
    if options.diff:
        all_args = [process_diff(all_args[0], all_args[1])]
    
    for filename in all_args:
        process_file(filename)


if __name__ == "__main__":
    sys.exit(main())
