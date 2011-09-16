import sys
import pysvn
import time
import os
import threading
from Queue import Queue


def warn(str):
    print >>sys.stderr, str


def notify(str):
    print >>sys.stderr, str


def retry(cmd):
    attempts = 1
    while attempts < 3:
        try:
            return cmd()
        except pysvn.ClientError, ex:
            warn('Got a ClientError in %s, retrying' % cmd.__name__)
            warn('Exception was: %s' % ex)
        attempts += 1
    return cmd()


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
                warn('Exception processing item %s' % (item,))
                warn('Exception was: %s' % ex)
            self.queue.task_done()
    
    def process(self, item):
        url,rev,dest = item
        warn('Copying: %s' % dest)
        def get_data():
            return self.client.cat(url, revision=rev, peg_revision=rev)
        data = retry(get_data)
        f = open(dest, 'wb')
        f.write(data)
        f.close()
        #warn('Copied: %s' % dest)
    

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
        #warn('Queuing copy: %s (queue size is %d)' % (dest, self.queue.qsize()))
    
    def finish(self):
        self.queue.join()


class Replayer(object):
    def __init__(self, source_client, dest_client, worker_clients, source_url, dest_url, start_rev, end_rev):
        self.source_client = source_client
        self.dest_client = dest_client
        self.cat_pool = CatPool(worker_clients)
        
        self.source_url = source_url
        self.dest_url = dest_url
        self.start_rev = start_rev
        self.end_rev = end_rev
        
        self.ignore_paths = ['/trunk/sc2/content/addons/3dovideo', '/trunk/sc2/content/addons/3dovoice']
        
        self.source_root = self.get_root(source_url, source_client)
        notify('Source root: %s' % self.source_root)
        self.source_rel_path = source_url[len(self.source_root):]
        notify('Source rel path: %s' % self.source_rel_path)
        
        self.dest_root = self.get_root(dest_url, dest_client)
        notify('Dest root: %s' % self.dest_root)
        self.dest_rel_path = dest_url[len(self.dest_root):]
        notify('Dest rel path: %s' % self.dest_rel_path)
        
        self.name = self.dest_rel_path.replace('/branches/', '')
    
    def get_root(self, url, client):
        entry = client.info2(url, recurse=False)[0][1]
        return entry.repos_root_URL
    
    def get_dest_path(self, path):
        if not path.startswith(self.source_rel_path):
            raise Exception('Path %s does not start with %s' % (path, self.source_rel_path))
        return self.dest_rel_path + path[len(self.source_rel_path):]
    
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
                return self.source_client.info2(self.source_root + path, recurse=False, revision=rev, peg_revision=rev)[0][1]
            
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
                warn('Copy source rev %d mapped to %d in dest' % (from_rev, to_rev))
                self.dest_client.copy(self.dest_url + '/' + local_from_path, local_path, pysvn.Revision(pysvn.opt_revision_kind.number, to_rev))
                if entry.kind != pysvn.node_kind.dir:
                    os.remove(local_path)
                    self.cat_pool.enqueue_copy(self.source_root + path, rev, local_path)
            else:
                if entry.kind == pysvn.node_kind.dir:
                    os.mkdir(local_path)
                    self.dest_client.add(local_path)
                else:
                    open(local_path, 'wb').close()
                    self.dest_client.add(local_path)
                    os.remove(local_path)
                    self.cat_pool.enqueue_copy(self.source_root + path, rev, local_path)

        print 'A %s (copied from %s:%s)' % (path, copyfrom_path, copyfrom_rev.number)
    
    def replay_add(self, rev, path):
        local_path = self.get_local_path(path)
        status = self.get_status(local_path)
        if os.path.exists(local_path) and status.text_status == pysvn.wc_status_kind.added:
            warn('Using existing add')
            pass
        else:
            entry = self.source_client.info2(self.source_root + path, recurse=False, revision=rev, peg_revision=rev)[0][1]
            
            if entry.kind == pysvn.node_kind.dir:
                os.mkdir(local_path)
                self.dest_client.add(local_path)
            else:
                open(local_path, 'wb').close()
                self.dest_client.add(local_path)
                os.remove(local_path)
                self.cat_pool.enqueue_copy(self.source_root + path, rev, local_path)

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
            self.cat_pool.enqueue_copy(self.source_root + path, rev, local_path)
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
        message = '(%s) %s\n\nCopied from %s, rev. %s by %s @ %s' % (self.name, le.message.strip('\n'), self.source_url, le.revision.number, le.author, time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(le.date)))
        global commit_notifications
        commit_notifications = 0
        def callback_notify(event_dict):
            global commit_notifications
            commit_notifications += 1
            notify('Progress (%d/%d): %s on %s' % (commit_notifications, len(self.actions), event_dict['action'], event_dict['path']))
        prev_notify = self.dest_client.callback_notify
        self.dest_client.callback_notify = callback_notify
        result = self.dest_client.checkin('.', message)
        self.dest_client.callback_notify = prev_notify
        self.dest_client.revpropset('replay:source-rev', str(le.revision.number), self.dest_url, result)
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
            if any([p.path.startswith(ip) for ip in self.ignore_paths]):
                warn('Skipping ignored change: %s' % p.path)
                continue
            dest_path = self.get_dest_path(p.path)
            notify('Replaying %s on %s to %s' % (p.action, p.path, dest_path))
            self.actions[self.get_local_path(p.path)] = p.action
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
        notify('Checking sanity of working copy')
        self.sanity_check()
        if len(self.actions) > 0:
            self.commit(le)
            pass
        else:
            notify('No actions in revision; skipping commit.')
    
    def run(self):
        notify('Building rev map')
        self.rev_map = {}
        results = self.dest_client.log(self.dest_url, discover_changed_paths=False, revprops=['replay:source-rev'])
        for r in results:
            if r.revprops is None:
                continue
            from_rev = int(r.revprops['replay:source-rev'])
            to_rev = r.revision.number
            self.rev_map[from_rev] = to_rev
        
        notify('Fetching logs')
        results = self.source_client.log(self.source_url, revision_start=self.start_rev, revision_end=self.end_rev, discover_changed_paths=True)
        for r in results:
            if r.revision.number in self.rev_map:
                raise Exception('Revision %d appears to already have been replayed here!' % r.revision.number)
            self.replay_log_entry(r)


def main(args):
    source_url = args[1] #'http://sc2.svn.sourceforge.net/svnroot/sc2/trunk/'
    #dest_url = 'https://project6014.googlecode.com/svn/branches/uqm-import/'

    start_rev = pysvn.Revision(pysvn.opt_revision_kind.number, args[2])
    end_rev = pysvn.Revision(pysvn.opt_revision_kind.number, args[3])
    
    password = args[4]

    source_client = pysvn.Client()
    dest_client = pysvn.Client()
    dest_client.callback_ssl_server_trust_prompt = lambda trust_dict: (True, 0, True)
    dest_client.callback_get_login = lambda realm, username, may_save: (True,  'ejrh00', password, True)
    
    dest_url = dest_client.info('.').url
    
    worker_clients = [pysvn.Client(), pysvn.Client(), pysvn.Client(), pysvn.Client()]

    r = Replayer(source_client, dest_client, worker_clients, source_url, dest_url, start_rev, end_rev)
    r.run()


if __name__ == '__main__':
    exit(main(sys.argv))
