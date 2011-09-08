import sys
import os
import re
import threading
import wx

DEBUGGING = True


def debug_print(str):
    if DEBUGGING:
        print >>sys.stderr, str


class ImageCacheWorker(threading.Thread):
    """A thread which preloads images into the cache in the background."""
    
    def __init__(self, cache):
        threading.Thread.__init__(self)
        self.cache = cache
        self.queue = []
        self.semaphore = threading.Semaphore(0)
    
    def run(self):
        while True:
            self.semaphore.acquire()
            try:
                filename = self.queue.pop(0)
                self.cache.load(filename)
            except IndexError:
                pass
    
    """Tell the worker to add a filename to its queue."""
    def enqueue(self, filename):
        if filename in self.queue:
            return
        
        self.queue.append(filename)
        self.semaphore.release()


class ImageCache(object):
    """A cache of images.  Keys are filenames, values are wx bitmaps, ready for painting to a DC."""
    
    def __init__(self, max=100*1000*1000):
        self.cache = {}
        self.worker = ImageCacheWorker(self)
        self.worker.daemon = True
        self.worker.start()
        self.memory_size = 0
        self.lru = []
        self.max_size = max
    
    def prefetch(self, filename):
        """Tell the cache to prefetch the specified image file if possible."""
        if filename is None or filename in self.cache:
            return
        
        self.worker.enqueue(filename)
    
    def load(self, filename):
        if filename in self.cache:
            return
        
        if self.memory_size > self.max_size and len(self.lru) > 2:
            old_filename = self.lru.pop(0)
            old_bmp = self.cache[old_filename]
            self.memory_size -= old_bmp.GetSize()[0] * old_bmp.GetSize()[1] * old_bmp.GetDepth() / 8
            del self.cache[old_filename]
            del old_bmp
        
        bmp = wx.Image(filename).ConvertToBitmap()
        self.cache[filename] = bmp
        debug_print('loaded %s' % filename)
        self.memory_size += bmp.GetSize()[0] * bmp.GetSize()[1] * bmp.GetDepth() / 8
        debug_print('memory size %d' % self.memory_size)
        self.lru.append(filename)
        
    def get(self, filename):
        """Get the image for this filename from the cache."""
        
        if filename is None:
            return None
        
        if filename not in self.cache:
            self.load(filename)
        
        self.lru.remove(filename)
        self.lru.append(filename)
        return self.cache[filename]


class ImageList(object):
    """A circular list of image filenames, loaded from a specified directory."""
    
    FILENAME_PATTERN = r'.+\.(png|gif|jpeg|jpe|jpg|bmp)'
    
    def __init__(self, filename=None):
        self.filename_re = re.compile(self.FILENAME_PATTERN, re.I)
        
        if filename is None:
            filename = os.getcwd()
        
        if os.path.isdir(filename):
            self.root = filename
            current_name = None
        else:
            self.root = os.path.dirname(filename)
            current_name = os.path.basename(filename)
        
        self.images = []
        self.current_pos = 0
        
        self.refresh()
        
        if current_name is not None:
            self.current_pos = self.images.index(current_name)
    
    def is_image(self, filename):
        return self.filename_re.match(filename)
    
    def full_path(self, filename):
        return os.path.join(self.root, filename)
    
    def refresh(self):
        current_name = self.current()
        self.images = filter(self.is_image, map(self.full_path, os.listdir(self.root)))
        try:
            self.current_pos = self.images.index(current_name)
        except ValueError:
            if self.current_pos >= len(self.images):
                self.current_pos = len(self.images) - 1
            elif self.current_pos < 0 and len(self.images) > 0:
                self.current_pos = 0
    
    def current(self):
        if len(self.images) == 0:
            return None
        return self.images[self.current_pos]
    
    def next(self):
        if len(self.images) == 0:
            return None
        self.current_pos += 1
        if self.current_pos >= len(self.images):
            self.current_pos = 0
        return self.images[self.current_pos]
    
    def prev(self):
        if len(self.images) == 0:
            return None
        self.current_pos -= 1
        if self.current_pos < 0:
            self.current_pos = len(self.images)-1
        return self.images[self.current_pos]
    
    def peek(self):
        if len(self.images) == 0:
            return None
        next_pos = self.current_pos + 1
        if next_pos >= len(self.images):
            next_pos = 0
        return self.images[next_pos]


class MainFrame(wx.Frame):
    def __init__(self, parent=None, ID=-1, pos=wx.DefaultPosition, size=wx.Size(800, 600), initial_filename=None):
        wx.Frame.__init__(self, parent, ID, 'Image viewer', pos, size, style=wx.DEFAULT_FRAME_STYLE)
        
        self.SetBackgroundColour(wx.BLACK)
        
        self.panel = wx.ScrolledWindow(self, style=wx.WANTS_CHARS)
        
        self.panel.Bind(wx.EVT_KEY_DOWN, self.OnKeyDown)
        self.panel.Bind(wx.EVT_PAINT, self.OnPaint)
        self.panel.Bind(wx.EVT_SCROLLWIN, self.OnScrollWin)
        
        self.panel.SetScrollRate(25, 25)
        
        self.full_screen = False
        
        TIMER_ID = wx.NewId()
        self.timer = wx.Timer(self, TIMER_ID)
        self.Bind(wx.EVT_TIMER, self.OnHideText)
        
        self.cache = ImageCache()
        self.list = ImageList(initial_filename)
        #for filename in self.list.images[:10]:
        #    self.cache.prefetch(filename)
        self.current = self.list.current()
        self.bmp = None
        self.DrawImage()

    def OnScrollWin(self, evt):
        #TODO scroll more smoothly
        if evt.GetOrientation() == wx.VERTICAL:
            debug_print('scroll pos %d' % self.panel.GetViewStart()[1])
        evt.Skip()

    def OnKeyDown(self, evt):
        if evt.GetKeyCode() == wx.WXK_F5:
            self.RefreshImage()
        elif evt.GetKeyCode() == wx.WXK_F11:
            self.ToggleFullScreen()
        elif evt.GetKeyCode() == wx.WXK_LEFT:
            self.current = self.list.prev()
            self.DrawImage()
        elif evt.GetKeyCode() == wx.WXK_RIGHT:
            self.current = self.list.next()
            self.DrawImage()
        elif evt.GetKeyCode() == wx.WXK_ESCAPE:
            self.Close()
        else:
            evt.Skip()

    def OnPaint(self, evt):
        if self.bmp is not None:
            dc = wx.PaintDC(self.panel)
            self.panel.PrepareDC(dc)
            dc.DrawBitmap(self.bmp, 0, 0)
            if self.show_text:
                text = "%s" % self.current
                px, py = 0, 0
                dc.SetTextForeground(wx.BLACK)
                dc.DrawText(text, px, py-1)
                dc.DrawText(text, px, py+1)
                dc.DrawText(text, px-1, py)
                dc.DrawText(text, px+1, py)
                dc.SetTextForeground(wx.WHITE)
                dc.DrawText(text, px, py)
            del dc
    
    def RefreshImage(self):
        self.list.refresh()
        #for filename in self.list.images[:10]:
        #    self.cache.prefetch(filename)
        self.current = self.list.current()
        self.DrawImage()
    
    def DrawImage(self):
        if self.current is not None:
            self.cache.prefetch(self.list.peek())
            self.bmp = self.cache.get(self.current)
            self.panel.SetVirtualSize(self.bmp.GetSize())
            self.show_text = True
            self.timer.Start(10*1000, True)
            debug_print('drew %s' % self.current)
        self.Refresh()

    def OnHideText(self, evt):
        self.show_text = False
        self.Refresh()
    
    def ToggleFullScreen(self):
        self.full_screen = not self.full_screen
        self.ShowFullScreen(self.full_screen)


if __name__ == '__main__':
    if len(sys.argv) > 1:
        initial_filename = sys.argv[1]
    else:
        initial_filename = None
    app = wx.PySimpleApp()
    frame = MainFrame(initial_filename)
    frame.Show(1)
    app.MainLoop()
