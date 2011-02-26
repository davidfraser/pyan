import math
import colorsys

import wx


class WxLayoutRenderer(object):
    def __init__(self):
        self.zoom = 1
        self.scroll_x = 0
        self.scroll_y = 0
        self.selected_node = None
        self.show_bounding = True

    def render(self):
        self.width = self.dc.GetSize().x
        self.height =self.dc.GetSize().y
        self.gc.Translate(self.width/2, self.height/2)
        self.gc.Translate(self.scroll_x, self.scroll_y)
        self.gc.Scale(self.zoom, self.zoom)
        self.lines = []
        self.circles = []
        self.texts = []
        
        self.render_node(self.layout.root)
        
        self.flush_queues()

    def coord_to_screen(self, x, y):
        x = x * self.zoom + self.width/2 + self.scroll_x
        y = y * self.zoom + self.height/2 + self.scroll_y
        return x, y
        
    def screen_to_coord(self, x, y):
        x = (x - self.width/2 - self.scroll_x) / self.zoom
        y = (y - self.height/2 - self.scroll_y) / self.zoom
        return x, y

    def draw_circle(self, x, y, r):
        self.gc.DrawEllipse(x-r,y-r, r*2, r*2);
        #self.gc.DrawCircle(bx*self.zoom,by*self.zoom, node.boudning_radius*self.zoom);        

    def flush_queues(self):
        p = wx.Pen("black")
        for x1,y1,x2,y2,thickness in self.lines:
            p.SetWidth(thickness)
            self.gc.SetPen(p)
            path = self.gc.CreatePath()
            path.MoveToPoint(x1,y1)
            path.AddLineToPoint(x2,y2)
            self.gc.StrokePath(path)
            #self.gc.DrawLine(x*self.zoom, y*self.zoom, px*self.zoom, py*self.zoom)

        self.gc.SetPen(wx.TRANSPARENT_PEN)
        for x,y,rad,bg in self.circles:
            self.gc.SetBrush(wx.Brush(wx.Color(bg[0],bg[1],bg[2])))
            self.draw_circle(x, y, rad)
        
        self.gc.SetPen(wx.BLACK_PEN)
        for t,x,y,ts in self.texts:
            font = wx.Font(ts, wx.FONTFAMILY_SWISS, wx.FONTSTYLE_NORMAL, wx.FONTWEIGHT_NORMAL)
            self.gc.SetFont(font)
            extx,exty = self.gc.GetTextExtent(t)
            self.gc.DrawText(t,x-extx/2,y-exty/2)
            #path = self.gc.CreatePath()
            #path.MoveToPoint(x-10,y-10)
            #path.AddLineToPoint(x+10,y+10)
            #path.MoveToPoint(x-10,y+10)
            #path.AddLineToPoint(x+10,y-10)
            #self.gc.StrokePath(path)

    def render_node(self, node, parent_hue=None):
        if parent_hue is not None:
            #new_hue = parent_hue + (random.random() - random.random() + (node.theta/ (2.0*math.pi)))/10
            new_hue = parent_hue + (node.theta/ (2.0*math.pi))/5
            while new_hue < 0:
                new_hue += 1.0
            while new_hue > 1:
                new_hue -= 1.0
            lum,sat = 0.8,0.5
        else:
            new_hue = 0
            lum,sat = 0.8,0.0
        
        try:
            new_hue = float(node.properties['hue'])
            sat = 0.5
        except KeyError:
            pass
        
        if node == self.selected_node:
            lum = 0.5
            rgb = colorsys.hls_to_rgb(new_hue, 0.5, 1)
        
        rgb = colorsys.hls_to_rgb(new_hue, lum, sat)
            
        r,g,b = rgb[0]*255,rgb[1]*255,rgb[2]*255
        
        x,y = self.layout.positions[node]
        
        if node.parent != None:
            px,py = self.layout.positions[node.parent]
            if (px-x)*(px-x) +(py-y)*(py-y) > 1:
                self.lines.append((x,y,px,py, node.height+1))
        
        # Check if it's too small to be worth drawing properly
        TOO_SMALL_THRESHOLD=20
        if len(node.children) != 0 and node.bounding_radius*self.zoom < TOO_SMALL_THRESHOLD:
            bx,by = self.layout.bounding_positions[node]
            r = (r + 255)/2
            g = (g + 255)/2
            b = (b + 255)/2
            self.circles.append((bx, by, node.bounding_radius, (r,g,b)))
            return
        
        # Check if it's off the size of the screen and not worth drawing at all, except for its line
        bx,by = self.layout.bounding_positions[node]
        scrx,scry = self.coord_to_screen(bx+node.bounding_radius,by+node.bounding_radius)
        if scrx < 0 or scry < 0:
            return
        scrx,scry = self.coord_to_screen(bx-node.bounding_radius,by-node.bounding_radius)
        if scrx > self.width or scry > self.height:
            return
        
        self.circles.append((x, y, node.inner_radius, (r,g,b)))

        if len(node.children) != 0 and self.show_bounding:
            bx,by = self.layout.bounding_positions[node]
            self.gc.SetBrush(wx.TRANSPARENT_BRUSH)
            self.gc.SetPen(wx.LIGHT_GREY_PEN)
            self.draw_circle(bx, by, node.bounding_radius)
        
        if node.parent != None:
            px,py = self.layout.positions[node.parent]
            if (px-x)*(px-x) +(py-y)*(py-y) > 1:
                self.lines.append((x,y,px,py, node.height+1))

        if len(node.content) != 0:
            ls = node.content.split('\n')
            ty = y - (20*(len(ls)-1)/2.0)*node.scale
            ly = 20*node.scale
            ts = 12*node.scale
            i = 0
            for l in ls:
                self.texts.append((l, x, ty + ly*i, ts))
                i += 1
        
        for c in node.children:
            if parent_hue is None:
                k = 0#random.random()
                new_hue = k + c.theta / (2.0*math.pi)
            self.render_node(c, new_hue)
