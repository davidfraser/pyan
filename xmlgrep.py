import sys
import libxml2
from glob import glob
from optparse import OptionParser

try:
    s = set()
except NameError:
    from sets import Set as set

try:
    from multiprocessing import Pool
except ImportError:
    import itertools
    class Pool(object):
        def __init__(self, num, init_func, args):
            init_func(*args)
        def imap(self, work_func, l):
            return itertools.imap(work_func, l)

def get_file(filename):
    doc = libxml2.readFile(filename, None, 0)

    def remove_ns(c):
        c.setNs(None)
        c = c.get_children()
        while c != None:
            remove_ns(c)
            c = c.next

    remove_ns(doc)

    return doc

def grep_file(filename, path, options):
    r = []
    doc1 = get_file(filename)

    nodes = doc1.xpathEval(path)
    if options.matching:
        if nodes != []:
            r.append('%s' % filename)
    elif options.unmatching:
        if nodes == []:
            r.append('%s' % filename)
    else:
        if type(nodes) in [int, float, str]:
            l = ''
            if not options.no_filename:
                l = l + '%s: ' % filename
            l = l + '%s' % nodes
            r.append(l)
        else:
            for n in nodes:
                l = ''
                if not options.no_filename:
                    l = l + '%s:%d: ' % (filename, n.lineNo())
                if len(options.value) == 0:
                    l = l + '%s' % n
                else:
                    bits = []
                    for vpath in options.value:
                        nodes = n.xpathEval(vpath)
                        if type(nodes) in [int, float, str]:
                            bits.append(str(nodes))
                        else:
                            bits.append('%s' % ''.join([str(n2) for n2 in nodes]))
                    l = l + ', '.join(bits)
                r.append(l)

    doc1.freeDoc()
    return r

def worker_init(path, options):
    globals()['path'] = path
    globals()['options'] = options

def worker_grep_file(f):
    try:
        return grep_file(f, path, options)
    except Exception, e:
        print >>sys.stderr, 'Failed on %s with %s' % (f, e)
        return False

def parse_command_line(argv = None):
    parser = OptionParser(usage="%prog [options] PATTERN FILENAME...\n       %prog -? (for help)", add_help_option=False)
    parser.add_option("-?", "--help", action='help',
                      help="display info about program usage, including options")
    parser.add_option("-l", "--matching", default=False, action='store_true',
                      help="print the name of each matching file")
    parser.add_option("-L", "--unmatching", default=False, action='store_true',
                      help="print the name of each unmatching file")
    parser.add_option("-h", "--no-filename", default=False, action='store_true',
                      help="suppress the prefixing filename on output")
    parser.add_option("--parallel", default=4, action='store',
                      help="number of grep processes to run in parallel")
    parser.add_option("-v", "--value", default=[], action='append',
                      help="XPath value to print for each match")
    (options, args) = parser.parse_args(argv[1:])

    if len(args) == 0:
        parser.error('No pattern specified')

    if options.no_filename and (options.matching or options.unmatching):
        parser.error('-h cannot be specified with -l or -L')

    options.parallel = int(options.parallel)

    return options, args

def main(argv=None):
    if argv is None:
        argv = sys.argv

    options, args = parse_command_line(argv)

    path = args[0]
    filenames = [b for a in args[1:] for b in glob(a)]

    pool = Pool(options.parallel, worker_init, (path, options))
    results = pool.imap(worker_grep_file, filenames)

    for r in results:
        if r is not False:
            for l in r:
                print l

if __name__ == "__main__":
    sys.exit(main())
