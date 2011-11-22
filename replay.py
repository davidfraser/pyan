import sys
import pysvn
import time
import os
import threading
from Queue import Queue
from optparse import OptionParser


NUM_CAT_WORKERS = 3
MAX_REVISIONS = 100


REPLAY_SOURCE = 'replay:source'
REPLAY_SOURCE_REV = 'replay:source-rev'
REPLAY_IGNORE = 'replay:ignore'


def warn(str):
    print >>sys.stderr, 'Warning: %s\n' % str,


def notify(str):
    print >>sys.stderr, '%s\n' % str,


def retry(cmd):
    attempts = 1
    while attempts < 3:
        try:
            return cmd()
        except pysvn.ClientError, ex:
            warn('Retrying after ClientError in %s: %s' % (cmd.__name__, ex))
        attempts += 1
    return cmd()


def join_paths(p1, p2):
    p = p1
    if not p.endswith('/'):
        p += '/'
    p += p2
    return p


class CatWorker(threading.Thread):
    def __init__(self, client, queue):
        threading.Thread.__init__(self)
        self.client = client
        self.queue = queue
    
    def run(self):
        while True:
            item = self.queue.get()
            try:
                self.process(item)
            except Exception, ex:
                warn('Processing item %s: %s' % (item, ex))
            self.queue.task_done()
    
    def process(self, item):
        url,rev,dest = item
        notify('Copying: %s' % dest)
        def get_data():
            return self.client.cat(url, revision=rev, peg_revision=rev)
        data = retry(get_data)
        f = open(dest, 'wb')
        f.write(data)
        f.close()
        #notify('Copied: %s' % dest)
    

class CatPool(object):    
    def __init__(self, clients):
        self.workers = []
        self.queue = Queue()
        for c in clients:
            w = CatWorker(c, self.queue)
            w.daemon = True
            self.workers.append(w)
            w.start()
    
    def enqueue_copy(self, url, rev, dest):
        item = url, rev, dest
        self.queue.put(item)
        #notify('Queuing copy: %s (queue size is %d)' % (dest, self.queue.qsize()))
    
    def finish(self):
        self.queue.join()


class Replayer(object):
    def __init__(self, options, source_client, dest_client, worker_clients):
        self.options = options
        
        self.source_client = source_client
        self.dest_client = dest_client
        self.cat_pool = CatPool(worker_clients)
        
        self.dest_url = dest_client.info('.').url
        
        self.ignore_paths = []
        
        self.dest_root = self.get_root(self.dest_url, dest_client)
        notify('Dest root: %s' % self.dest_root)
        self.dest_rel_path = self.dest_url[len(self.dest_root):]
        notify('Dest rel path: %s' % self.dest_rel_path)
        
        self.name = self.dest_rel_path.replace('/branches/', '')
    
    def get_root(self, url, client):
        entry = client.info2(url, recurse=False)[0][1]
        return entry.repos_root_URL
    
    def get_dest_path(self, path):
        if not path.startswith(self.source_rel_path):
            raise Exception('Path %s does not start with %s' % (path, self.source_rel_path))
        return join_paths(self.dest_rel_path, path[len(self.source_rel_path):])
    
    def get_local_path(self, path):
        if not path.startswith(self.source_rel_path):
            raise Exception('Path %s does not start with %s' % (path, self.source_rel_path))
        return path[len(self.source_rel_path):]
    
    def copy_contents(self, url, rev, dest, client):
        def get_data():
            return client.cat(url, revision=rev, peg_revision=rev)
        data = self.retry(get_data)
        f = open(dest, 'wb')
        f.write(data)
        f.close()
    
    def next_rev(self, rev):
        next_rev = None
        for r in self.rev_map.keys():
            if r > rev:
                if next_rev is None or next_rev > r:
                    next_rev = r
            
        return next_rev
        
    def get_status(self, path):
        results = self.dest_client.status(path, recurse=False)
        if len(results) > 0:
            results.sort(key=lambda x: x.path)
            return results[0]
        else:
            return None

    def replay_copy(self, rev, path, copyfrom_rev, copyfrom_path):
        local_path = self.get_local_path(path)
        if os.path.exists(local_path):
            status = self.get_status(local_path)
        else:
            status = None
        if os.path.exists(local_path) and status.text_status == pysvn.wc_status_kind.added and status.entry.copy_from_revision is not None:
            warn('Using existing copy')
            pass
        elif os.path.exists(local_path) and status.text_status == pysvn.wc_status_kind.replaced and status.entry.copy_from_revision is not None:
            warn('Using existing replace')
            pass
        else:
            def get_entry():
                return self.source_client.info2(join_paths(self.source_root, path), recurse=False, revision=rev, peg_revision=rev)[0][1]
            
            entry = retry(get_entry)
            
            do_copy = True
            from_rev = copyfrom_rev.number
            if not copyfrom_path.startswith(self.source_rel_path):
                if entry.kind == pysvn.node_kind.dir:
                    raise Exception('Complex directory copies not supported yet (%s:%s)!' % (copyfrom_path, copyfrom_rev.number))
                warn('Eliding copy from outside source: %s' % copyfrom_path)
                do_copy = False
            elif from_rev not in self.rev_map:
                if entry.kind == pysvn.node_kind.dir:
                    raise Exception('Complex directory copies not supported yet (%s:%s)!' % (copyfrom_path, copyfrom_rev.number))
                new_from_rev = self.next_rev(from_rev)
                if new_from_rev is None:
                    warn('Eliding copy from future revision: %s' % copyfrom_path)
                    do_copy = False
                else:
                    warn('Finessing copy from missed revision: %d to %d' % (from_rev, new_from_rev))
                    from_rev = new_from_rev
            
            if do_copy:
                to_rev = self.rev_map[from_rev]
                local_from_path = self.get_local_path(copyfrom_path)
                notify('Copy source rev %d mapped to %d in dest' % (from_rev, to_rev))
                self.dest_client.copy(join_paths(self.dest_url, local_from_path), local_path, pysvn.Revision(pysvn.opt_revision_kind.number, to_rev))
                if entry.kind != pysvn.node_kind.dir:
                    os.remove(local_path)
                    self.cat_pool.enqueue_copy(join_paths(self.source_root, path), rev, local_path)
            else:
                if entry.kind == pysvn.node_kind.dir:
                    os.mkdir(local_path)
                    self.dest_client.add(local_path)
                else:
                    open(local_path, 'wb').close()
                    self.dest_client.add(local_path)
                    os.remove(local_path)
                    self.cat_pool.enqueue_copy(join_paths(self.source_root, path), rev, local_path)

        print 'A %s (copied from %s:%s)' % (path, copyfrom_path, copyfrom_rev.number)
    
    def replay_add(self, rev, path):
        local_path = self.get_local_path(path)
        status = self.get_status(local_path)
        if os.path.exists(local_path) and status.text_status == pysvn.wc_status_kind.added:
            warn('Using existing add')
            pass
        else:
            entry = self.source_client.info2(join_paths(self.source_root, path), recurse=False, revision=rev, peg_revision=rev)[0][1]
            
            if entry.kind == pysvn.node_kind.dir:
                os.mkdir(local_path)
                self.dest_client.add(local_path)
            else:
                open(local_path, 'wb').close()
                self.dest_client.add(local_path)
                os.remove(local_path)
                self.cat_pool.enqueue_copy(join_paths(self.source_root, path), rev, local_path)

        print 'A %s' % path
    
    def replay_modify(self, rev, path):
        local_path = self.get_local_path(path)
        status = self.get_status(local_path)
        if os.path.exists(local_path) and status.text_status == pysvn.wc_status_kind.modified:
            warn('Using existing modify')
            pass
        else:
            if os.path.exists(local_path):
                os.remove(local_path)
            self.cat_pool.enqueue_copy(join_paths(self.source_root, path), rev, local_path)
        print 'M %s' % path
    
    def replay_delete(self, path):
        local_path = self.get_local_path(path)
        status = self.get_status(local_path)
        if not os.path.exists(local_path) and status.text_status == pysvn.wc_status_kind.deleted:
            warn('Using existing delete')
            pass
        elif os.path.exists(local_path) and status.text_status == pysvn.wc_status_kind.replaced:
            warn('Using existing replace')
            pass
        else:
            self.dest_client.remove(local_path)
            
        print 'D %s' % path
    
    def sanity_check(self):
        """Check that the status entries match the log entries (within source_rel_path)."""
        results = self.dest_client.status('.', recurse=True, get_all=False)
        for r in results:
            if r.text_status == pysvn.wc_status_kind.unversioned:
                continue
            if r.text_status == pysvn.wc_status_kind.deleted:
                p = r.path
                while True:
                    try:
                        action = self.actions[p]
                        if action == 'D':
                            break
                    except KeyError:
                        if '/' not in p:
                            raise Exception("Deletion not in actions, nor is parent: %s"  % r.path)
                        p = p.rsplit('/', 1)[0]
                continue
            try:
                action = self.actions[r.path]
            except KeyError:
                raise Exception('Status not in actions: %s (action %s)' % (r.path, r.text_status))
            if r.text_status == pysvn.wc_status_kind.modified and action == 'M':
                continue
            if r.text_status == pysvn.wc_status_kind.added and action in  ['A', 'R']:
                continue
            if r.text_status == pysvn.wc_status_kind.deleted and action in  ['D', 'R']:
                continue
            if r.text_status == pysvn.wc_status_kind.replaced and action == 'R':
                continue
            raise Exception('Status conflict on path: %s (expected %s, was %s)' % (r.path, action, r.text_status))
        
    def commit(self, le):
        notify('Committing %d actions' % len (self.actions))
        message = le.message.strip('\n').decode('utf8')
        message = '(%s) %s\n\nCopied from %s, rev. %s by %s @ %s' % (self.name, message, self.source_url, le.revision.number, le.author, time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(le.date)))
        global commit_notifications
        commit_notifications = 0
        expected_notifications = len(self.actions) + len([k for k in self.actions.keys() if k != 'D'])
        def callback_notify(event_dict):
            global commit_notifications
            commit_notifications += 1
            notify('Progress (%d/%d): %s on %s' % (commit_notifications, expected_notifications, event_dict['action'], event_dict['path']))
        prev_notify = self.dest_client.callback_notify
        self.dest_client.callback_notify = callback_notify
        result = self.dest_client.checkin('.', message)
        self.dest_client.callback_notify = prev_notify
        self.dest_client.revpropset(REPLAY_SOURCE_REV, str(le.revision.number), self.dest_url, result)
        notify('Committed replayed revision %d as %d' % (le.revision.number, result.number))
        self.rev_map[le.revision.number] = result.number
    
    def replay_log_entry(self, le):
        notify('Replaying change %d (%d items)' % (le.revision.number, len(le.changed_paths)))
        
        self.actions = {}
        
        paths = le.changed_paths[:]
        paths.sort(key=lambda x: x.path)
        for p in paths:
            p.path = p.path.replace('\\', '/')
            if not p.path.startswith(self.source_rel_path):
                warn('Skipping outside change: %s' % p.path)
                continue
            local_path = self.get_local_path(p.path)
            if any([local_path.startswith(ip) for ip in self.ignore_paths]):
                warn('Skipping ignored change: %s' % p.path)
                continue
            dest_path = self.get_dest_path(p.path)
            notify('Replaying %s on %s to %s' % (p.action, p.path, dest_path))
            self.actions[local_path] = p.action
            try:
                if p.action == 'A' and p.copyfrom_revision is not None:
                    self.replay_copy(le.revision, p.path, p.copyfrom_revision, p.copyfrom_path)
                elif p.action == 'A':
                    self.replay_add(le.revision, p.path)
                elif p.action == 'M':
                    self.replay_modify(le.revision, p.path)
                elif p.action == 'R' and p.copyfrom_revision is not None:
                    self.replay_delete(p.path)
                    self.replay_copy(le.revision, p.path, p.copyfrom_revision, p.copyfrom_path)
                elif p.action == 'D':
                    self.replay_delete(p.path)
                else:
                    raise Exception('Unhandled action: %s' % p.action)
            except:
                warn('Exception processing item %s' % p.path)
                raise

        notify('Completing copies')
        self.cat_pool.finish()
        if self.options.skip_check:
            warn('Skipping sanity check')
        else:
            notify('Checking sanity of working copy')
            self.sanity_check()
        if not self.options.commit:
            warn('Not comitting')
        elif len(self.actions) > 0:
            self.commit(le)
        else:
            notify('No actions in revision; skipping commit.')
    
    def get_replay_config(self):
        # Get source URL
        results = self.dest_client.propget(REPLAY_SOURCE, self.dest_url, recurse=False)
        if self.dest_url in results:
            self.source_url = results[self.dest_url]
        else:
            raise Exception('Destination does not have %s set!' % REPLAY_SOURCE)
        
        self.source_root = self.get_root(self.source_url, self.source_client)
        notify('Source root: %s' % self.source_root)
        self.source_rel_path = self.source_url[len(self.source_root):]
        notify('Source rel path: %s' % self.source_rel_path)
        
        # Get ignore paths
        results = self.dest_client.propget(REPLAY_IGNORE, self.dest_url, recurse=False)
        if self.dest_url in results:
            self.ignore_paths.extend(results[self.dest_url].split(','))
    
    def init(self, source_url, source_rev):
        # Check it's not already marked as an import branch.
        results = self.dest_client.propget(REPLAY_SOURCE, self.dest_url, recurse=False)
        if self.dest_url in results:
            raise Exception('Destination is already an import path (from %s)!' % results[self.dest_url])
        
        message = 'Initialise %s as import branch of %s, initially from revision %d.' % (self.dest_rel_path, source_url, source_rev)
        
        self.dest_client.propset(REPLAY_SOURCE, source_url, '.')
        if len(self.options.ignore_paths) > 0:
            value = ','.join(self.options.ignore_paths)
            self.dest_client.propset(REPLAY_IGNORE, value, '.')
        result = self.dest_client.checkin('.', message, recurse=False)
        self.dest_client.revpropset(REPLAY_SOURCE_REV, str(source_rev), self.dest_url, result)
    
    def sync(self):
        self.get_replay_config()
        
        notify('Building rev map')
        self.rev_map = {}
        results = self.dest_client.log(self.dest_url, discover_changed_paths=False, revprops=[REPLAY_SOURCE_REV])
        for r in results:
            if r.revprops is None:
                continue
            from_rev = int(r.revprops[REPLAY_SOURCE_REV])
            to_rev = r.revision.number
            self.rev_map[from_rev] = to_rev
        
        last_rev = max(self.rev_map.keys())
        self.start_rev = pysvn.Revision(pysvn.opt_revision_kind.number, last_rev+1)
        self.end_rev = pysvn.Revision(pysvn.opt_revision_kind.head)
        
        notify('Fetching logs for revisions from %d' % self.start_rev.number)
        results = self.source_client.log(self.source_url, revision_start=self.start_rev, revision_end=self.end_rev, discover_changed_paths=True, limit=MAX_REVISIONS)
        results.sort(key=lambda x: x.revision.number)
        notify('Replaying revisions from %d to %d' % (results[0].revision.number, results[-1].revision.number))
        for r in results:
            if r.revision.number in self.rev_map:
                raise Exception('Revision %d appears to already have been replayed here!' % r.revision.number)
            self.replay_log_entry(r)


def main(args):
    usage = """usage: %prog init SOURCE-URL SOURCE-REV [--ignore PATH]... [-v]\n       %prog sync [--skip-check] [-c] [-v]"""
    desc = """Initialise a working copy as an import branch; a source url and revision must be specified.
Or, syncronise an import branch from its source."""
    parser = OptionParser(usage=usage, description=desc)
    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose", default=False,
                      help="print verbose status messages")
    parser.add_option("--skip-check",
                      action="store_true", dest="skip_check", default=False,
                      help="skip sanity check")
    parser.add_option("-c", "--commit",
                      action="store_true", dest="commit", default=False,
                      help="commit after replaying each revision")
    parser.add_option("--ignore", metavar="PATH",
                      action="append", dest="ignore_paths",
                      help="specify paths to ignore (should be relative to source url)")

    options, args = parser.parse_args()
    if len(args) == 0:
        parser.error('A command such as init or sync must be specified.')
    
    command = args[0]
    if command == 'init':
        if options.commit:
            parser.error('init command does not want --commit option')
        if options.skip_check:
            parser.error('init command does not want --skip-check option')
        if len(args) != 3:
            parser.error('init command wants exactly 2 arguments')
        source_url = args[1]
        source_rev = int(args[2])
    elif command == 'sync':
        if options.ignore_paths:
            parser.error('sync command does not want --ignore option')
        if len(args) != 1:
            parser.error('sync command wants no arguments')
    else:
        parser.error('Unknown command: %s' % command)
    
    source_client = pysvn.Client()
    dest_client = pysvn.Client()
    dest_client.callback_ssl_server_trust_prompt = lambda trust_dict: (True, 0, True)
    dest_client.callback_get_login = lambda realm, username, may_save: (True,  'ejrh00', password, True)
    
    worker_clients = [pysvn.Client() for x in range(NUM_CAT_WORKERS)]

    r = Replayer(options, source_client, dest_client, worker_clients)
    
    if command == 'init':
        r.init(source_url, source_rev)
    elif command == 'sync':
        r.sync()


if __name__ == '__main__':
    exit(main(sys.argv))
