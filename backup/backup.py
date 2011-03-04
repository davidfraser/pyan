"""This module implements a backup procedure, with optimisations
for NTFS drives.

Source can be any directory, for example C:/
Target is a directory, with each backup getting a subdirectory within it.
For example, C:/snapshots/20101103.

The target is essentially a copy of the source, except that:
  - some files and/or directories may have been excluded, and
  - files and/or directories that have not changed since the previous
    backup will be links (hard for file, sym for dirs to the copies
    in the previous backup dir).

Some state files are maintained in the base target dir:
  - journal for the state of the NTFS journal from the last
  backup, including a dir map.
  - previous for the previous successful backup name
  - exclusions is a list of files and dirs to exclude from backups.

Basic algorithm:
  - Input is source directory, target directory, and name.
  - Read exclusions file and journal file.
  - Open journal and build list of changed paths.
  - Iterate over source, using changed paths to optimise if possible.
     - For each file/dir, either copy in or make link to copy in previous
       backup.
  - Save journal file.
  
Example:

backup.py C:/ C:/snapshots 20101103
"""

import sys
import os
import os.path
import cPickle
import struct
import win32file
import winioctlcon


BUFFER_SIZE = 1024*1024

JOURNAL_FILENAME = "journal"
PREVIOUS_FILENAME = "previous"
EXCLUSIONS_FILENAME = "exclusions"

ALLOW_JOURNAL = True

if ALLOW_JOURNAL:
    try:
        import journal
        from journal import Journal
    except ImportError:
        ALLOW_JOURNAL = False


def unpickle_file(filename):
    f = open(filename, 'rb')
    obj = cPickle.load(f)
    f.close()
    return obj

def pickle_to_file(obj, filename):
    f = open(filename, 'wb')
    cPickle.dump(obj, f)
    f.close()


class ConsoleNotifier(object):
    def __init__(self, parent):
        self.parent = parent

    def notice(self, msg):
        print >>sys.stderr, msg
    
    def warning(self, msg):
        print >>sys.stderr, 'Warning: %s' % msg
    
    def error(self, msg, ex=None):
        print >>sys.stderr, 'Error: %s' % msg
        if ex is not None:
            print >>sys.stderr, 'Exception was:', ex


# Reference: http://msdn.microsoft.com/en-us/library/cc232007%28PROT.13%29.aspx
def make_reparse_point(dest, link):
    print dest, link
    if dest[-1] == '/' or dest[-1] == '\\':
        dest = dest[:len(dest)-1]
    if link[-1] == '/' or link[-1] == '\\':
        link = link[:len(link)-1]
    import journalcmd
    os.mkdir(dest)
    dirh = win32file.CreateFile(dest, win32file.GENERIC_READ | win32file.GENERIC_WRITE, 0, None, 
            win32file.OPEN_EXISTING, win32file.FILE_FLAG_OPEN_REPARSE_POINT| win32file.FILE_FLAG_BACKUP_SEMANTICS, None)
    tag = 0xA0000003L
    link = '\\??\\' + link
    link = link.encode('utf-16')[2:]
    datalen = len(link)
    inp = struct.pack('LHHHHHH', tag, datalen+8+4, 0, 0, datalen, datalen+2, 0) + link + '\0\0\x91\x7c'
    win32file.DeviceIoControl(dirh, winioctlcon.FSCTL_SET_REPARSE_POINT, inp, None)


class Backup(object):
    """A backup is the process of copying all current files in a drive
    into a backup location."""

    def __init__(self):
        self.name = None
        self.source = None
        self.target = None
        self.enable_journal = True
        self.enable_dir_reuse = False
    
    def flesh_out_paths(self, paths):
        if paths is None:
            return None
        drive = os.path.splitdrive(self.source)[0]
        new_paths = set()
        for p in paths:
            components = p.split('/')
            subpath = drive
            for c in components:
                subpath = subpath + '/' + c
                subpath = subpath.replace('//', '/')
                new_paths.add(subpath)
        self.notifier.notice('Fleshed out %d paths' % len(new_paths))
        return new_paths

    def copy_item(self, item_path):
        source_path = os.path.join(self.source, item_path)
        dest_path = os.path.join(self.target, self.name, item_path)
        f = open(source_path, 'rb')
        f2 = open(dest_path, 'wb')
        while True:
            buf = f.read(BUFFER_SIZE)
            if len(buf) == 0:
                break
            f2.write(buf)
        f.close()
        f2.close()
        self.notifier.notice('Copied: %s' % item_path)

    def reuse_item(self, item_path):
        source_path = os.path.join(self.source, item_path)
        dest_path = os.path.join(self.target, self.name, item_path)
        link_path = os.path.join(self.target, self.previous_name, item_path)
        if os.path.isfile(source_path):
            win32file.CreateHardLink(dest_path, link_path)
        else:
            try:    
                win32file.CreateSymbolicLink(dest_path, link_path, 1)   # SYMBOLIC_LINK_FLAG_DIRECTORY
            except NotImplementedError:
                make_reparse_point(dest_path, link_path)

    def make_dir(self, item_path):
        dest_path = os.path.join(self.target, self.name, item_path)
        os.mkdir(dest_path)

    def get_children(self, item_path):
        source_path = os.path.join(self.source, item_path)
        try:
            children = os.listdir(source_path)
        except WindowsError:
            self.notifier.warning('Unable to find children in %s' % source_path)
            children = []
        return children
    
    def is_excluded(self, item_path):
        return item_path in self.exclusions
    
    def is_reusable(self, item_path):
        if self.previous_name is None:
            return False
        if self.changed_paths is None:
            return False
        source_path = os.path.join(self.source, item_path)
        if os.path.isdir(source_path) and not self.enable_dir_reuse:
            return False
        if source_path[-1] == '/' or source_path[-1] == '\\':
            source_path = source_path[:len(source_path)-1]
        source_path = source_path.replace('\\', '/')
        self.notifier.notice('Checking for %s in changed paths' % source_path)
        #print self.changed_paths
        if source_path in self.changed_paths:
            self.notifier.notice('Found')
            return False
        self.notifier.notice('Not found')
        return True
    
    def backup_item(self, item_path):
        if self.is_excluded(item_path):
            return
        
        if self.is_reusable(item_path):
            self.reuse_item(item_path)
            return
        
        source_path = os.path.join(self.source, item_path)
        if os.path.isfile(source_path):
            self.copy_item(item_path)
        else:
            self.make_dir(item_path)
            for c in self.get_children(item_path):
                self.backup_item(os.path.join(item_path, c))
        self.notifier.notice('Backed up: %s' % item_path)

    def check_target(self):
        if not os.path.exists(self.target):
            self.notifier.notice('Creating new target: %s' % self.target)
            os.mkdir(self.target)
        
        if os.path.exists(os.path.join(self.target, self.name)):
            raise Exception, 'Target with name already exists!'

    def read_exclusions(self):
        self.exclusions = set()
        try:
            f = open(os.path.join(self.target, EXCLUSIONS_FILENAME), 'rt')
            for line in f:
                self.exclusions.add(line)
            f.close()
            self.notifier.notice('Read %d exclusions' % len(self.exclusions))
        except IOError:
            self.notifier.warning('Failed to read exclusions file')

    def open_journal(self):
        journal_filename = os.path.join(self.target, JOURNAL_FILENAME)
        try:
            self.journal_state = unpickle_file(journal_filename)
        except IOError:
            self.notifier.notice('Journal state not found, starting anew')
            self.journal_state = (None, None, {})
        drive = os.path.splitdrive(self.source)[0]
        self.journal = Journal(drive)
        self.journal.set_state(self.journal_state)
        self.notifier.notice('Opened journal')

    def close_journal(self):
        journal_filename = os.path.join(self.target, JOURNAL_FILENAME)
        self.journal_state = self.journal.get_state()
        pickle_to_file(self.journal_state, journal_filename)
        self.notifier.notice('Closed journal')
    
    def run(self):
        self.check_target()
        
        try:
            prev_filename = os.path.join(self.target, PREVIOUS_FILENAME)
            self.previous_name = unpickle_file(prev_filename)
        except IOError:
            self.previous_name = None
        
        self.read_exclusions()
        self.exclusions.add(self.target)
        
        if self.enable_journal:
            self.open_journal()
            self.journal.process()
            if self.journal.replay_all:
                self.changed_paths = None
            else:
                paths = self.journal.get_changed_paths()
                self.changed_paths = self.flesh_out_paths(paths)
        else:
            self.changed_paths = None      
        
        self.backup_item('')
        
        if self.enable_journal:
            self.close_journal()
        
        prev_filename = os.path.join(self.target, PREVIOUS_FILENAME)
        pickle_to_file(self.name, prev_filename)


def main(args=None):
    if args is None:
        args = sys.argv
    backup = Backup()
    backup.notifier = ConsoleNotifier(backup)
    if len(args) != 4:
        raise Exception, "Command line: backup.py SOURCE TARGET NAME"
    #TODO parse options to set these
    backup.source = args[1]
    backup.target = args[2]
    backup.name = args[3]
    #backup.enable_dir_reuse = True
    #backup.enable_journal = False
    backup.run()


if __name__ == '__main__':
    main()
