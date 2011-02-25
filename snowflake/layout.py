import sys
import math
import random
import justify
import colorsys
from tree import Node
import tree


class Layout(object):
    def __init__(self, root):
        self.root = root
        self.positions = {}

    def run(self):
        self.positions[self.root] = 0,0
        self.arrange_node(self.root)
        self.layout_node(self.root)

    def arrange_node(self, node, parent_angle=None):
        if parent_angle is not None:
            theta = math.pi+parent_angle
        else:
            theta = 0
        num_edges = len(node.children)
        if parent_angle is not None:
            num_edges += 1
        theta_increment = (2*math.pi)/num_edges
        for c in node.children:
            theta += theta_increment
            self.positions[c] = math.cos(theta), math.sin(theta)
            self.arrange_node(c, theta)

    def layout_node(self, node, parent_xy=(0,0), d=150):
        d2 = (node.height+1)
        x,y = self.positions[node]
        x = parent_xy[0] + x*d*d2
        y = parent_xy[1] + y*d*d2
        self.positions[node] = x,y
        
        for c in node.children:
            self.layout_node(c, (x,y), d)


def get_theta_delta(r1, r2, r3):
    """Return the delta in angle when spheres of radius r2 and r3 are packed next to a sphere of radius r1."""
    a = r1 + r2
    b = r1 + r3
    c = r2 + r3
    delta = math.acos((a*a + b*b - c*c) / float(2*a*b))
    #print >>sys.stderr, r1, r2, r3, delta
    return delta


def angularise(dx,dy):
    a = math.atan2(dy, dx)
    d = math.sqrt(dx*dx + dy*dy)
    return a,d


def deangularise(a,d):
    dx = math.cos(a)*d
    dy = math.sin(a)*d
    return dx,dy


def circumscribe(circs):
    if len(circs) == 1:
        return circs[0]
    
    rx = sum([x for x,y,r in circs])/float(len(circs))
    ry = sum([y for x,y,r in circs])/float(len(circs))
    rr = 0
    
    while True:
        circs.sort(key=lambda c: -math.sqrt((c[0]-rx)**2 + (c[1]-ry)**2) - c[2])
        cx,cy,cr = circs[0]
        d = math.sqrt((cx-rx)**2 + (cy-ry)**2)
        if rr >= d+cr:
            break
        
        t = rr + d + cr
        p = t/2 - rr
        nx = rx + (p/d)*(cx-rx)
        ny = ry + (p/d)*(cy-ry)
        nr = rr + 1
        rx,ry,rr = nx,ny,nr
    
    #print >>sys.stderr, circs, "->", (rx,ry,rr)
    return rx,ry,rr


def alternate(list):
    if len(list) <= 1:
        return list
    i = 0
    l1 = []
    l2 = []
    for c in list:
        if i % 2 == 0:
            l1.append(c)
        else:
            l2.append(c)
        i += 1  
    return alternate(l1) + alternate(l2)


class CircleLayout(object):
    def __init__(self, root):
        self.root = root
        self.positions = {}
        self.bounding_positions = {}
    
    def get_text_size(self, s, sc=1):
        lines = s.split('\n')
        m = max([len(l) for l in lines])
        m = math.sqrt(m*m + (len(lines)*2)**2)
        return (m*4.75 + 14) * sc + 8

    def get_changed(self, node):
        changed = set()
        while node is not None:
            changed.add(node)
            node = node.parent
        
        return changed

    def run(self, changed_node=None):
        if changed_node is not None:
            self.changed = self.get_changed(changed_node)
        else:
            self.changed = None
            self.positions = {}
            self.bounding_positions = {}
        self.positions[self.root] = 0,0
        self.arrange_node(self.root)
        self.layout_node(self.root)

    def arrange_node(self, node):
        if self.changed is not None and node not in self.changed:
            return
        
        if '\n' not in node.content and len(node.content) > 0:
            points = justify.get_points(node.content)
            all_js = justify.justify_text(points, 2)
            j = all_js[0][1]
            node.content = justify.render_text(node.content, j)
        
        node.scale = 1.125**node.height
        margin = 10*node.scale
        node.inner_radius = (self.get_text_size(node.content, node.scale)+12)/2
        node.bounding_radius = node.inner_radius + margin
        node.inner_theta, node.inner_dist = 0,0
        node.in_theta = 0
        node.dist = 0
        node.theta = 0
        
        if len(node.children) == 0:
            return
        
        for c in node.children:
            self.arrange_node(c)
        
        queue = list(node.children)
        #queue.sort(key=lambda n: -n.bounding_radius)
        #if node.parent is None:
        #    #print >>sys.stderr, [c.bounding_radius for c in queue]
        #    queue = alternate(queue)
        #    #print >>sys.stderr, [c.bounding_radius for c in queue]
        
        check_rad = node.inner_radius
        while True:
            theta_up, theta_down = 0,1
            for c in queue:
                c.dist = check_rad + c.bounding_radius
                if theta_up < theta_down:
                    d = get_theta_delta(check_rad, c.bounding_radius, c.bounding_radius)
                    d1 = d
                    c.theta = theta_up
                    theta_down = theta_up
                    up_rad = c.bounding_radius
                    down_rad = c.bounding_radius
                #elif node.parent is not None and up_rad > down_rad:
                #    d = get_theta_delta(check_rad, up_rad, c.bounding_radius)
                #    theta_up += d
                #    c.theta = theta_up
                #    up_rad = c.bounding_radius
                else:
                    d = get_theta_delta(check_rad, down_rad, c.bounding_radius)
                    theta_down -= d
                    c.theta = theta_down
                    down_rad = c.bounding_radius
            
            if (theta_up + d) - (theta_down - d) < 2.0*math.pi:
            #if (theta_up) - (theta_down) < math.pi:
                break
            
            check_rad *= 1.1
            
        node.in_theta = (theta_up + theta_down) / 2
        
        if node.parent is None:
            tadj = 2*math.pi - (theta_up - theta_down) - d/2 - d1/2
            adj = tadj / len(queue)
            #print >>sys.stderr, theta_up, theta_down, tadj, adj
            sorted_queue = queue[:]
            sorted_queue.sort(key=lambda c: c.theta)
            cumadj = 0
            for c in sorted_queue:
                c.theta += cumadj
                cumadj += adj
        
        #print >>sys.stderr, "in_theta", node.in_theta
        
        circs = [(0,0,node.inner_radius)] + [(c.dist*math.cos(c.theta), c.dist*math.sin(c.theta), c.bounding_radius) for c in queue]
        #print >>sys.stderr, circs
        cx,cy,node.bounding_radius = circumscribe(circs)
        node.bounding_radius += margin
        #print >>sys.stderr, cx,cy
        node.inner_theta, node.inner_dist = angularise(cx,cy)

    def layout_node(self, node, bxy=(0,0), theta=0):
        self.bounding_positions[node] = bxy
        bx,by = bxy
        theta -= node.in_theta
        x1,y1 = deangularise(theta + node.inner_theta, node.inner_dist)
        x = bx - x1
        y = by - y1
        self.positions[node] = x,y
        for c in node.children:
            x2,y2 = deangularise(theta + c.theta, c.dist)
            cx = x + x2
            cy = y + y2
            self.layout_node(c, (cx,cy), theta + c.theta)

    def find_node(self, x,y, current=None):
        if current is None:
            current = self.root
        bx,by = self.bounding_positions[current]
        if (x-bx)*(x-bx) + (y-by)*(y-by) > current.bounding_radius*current.bounding_radius:
            return None
        
        cx,cy = self.positions[current]
        if (x-cx)*(x-cx) + (y-cy)*(y-cy) <= current.inner_radius*current.inner_radius:
            return current
        
        for c in current.children:
            n = self.find_node(x,y, c)
            if n is not None:
                return n
        
        return None


class DotRenderer(object):
    def __init__(self, layout):
        self.layout = layout

    def render(self):
        print "digraph G {"
        self.render_node(self.layout.root)
        print "}"

    def render_node(self, node):
        num = hash(node)
        if node != self.layout.root:
            pnum = hash(node.parent)
            print "    node_%d -> node_%d" % (pnum, num)
        x,y = self.layout.positions[node]
        print "    node_%d [label=\"%s\", pos=\"%f, %f\"]" % (num, node.content, x, y)
        
        for c in node.children:
            self.render_node(c)


class SvgRenderer(object):
    def __init__(self, layout):
        self.layout = layout
        self.show_bounding = False

    def render(self):
        self.offset_x = self.layout.root.bounding_radius
        self.offset_y = self.offset_x
        width = self.offset_x * 2
        height = width
        self.lines = []
        self.circles = []
        self.texts = []
        
        print """<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg width="%f" height="%f" version="1.1" xmlns="http://www.w3.org/2000/svg">""" % (width, height)
        
        
        self.render_node(self.layout.root)
        
        for l in self.lines:
            print l
        
        for c in self.circles:
            print c
        
        for t in self.texts:
            print t
        
        print "</svg>"

    def render_node(self, node, parent_hue=None):
        if parent_hue is not None:
            #new_hue = parent_hue + (random.random() - random.random() + (node.theta/ (2.0*math.pi)))/10
            new_hue = parent_hue + (node.theta/ (2.0*math.pi))/5
            while new_hue < 0:
                new_hue += 1.0
            while new_hue > 1:
                new_hue -= 1.0
            rgb = colorsys.hls_to_rgb(new_hue, 0.8, 0.8)
        else:
            new_hue = 0
            rgb = colorsys.hls_to_rgb(new_hue, 0.8, 0)
        x,y = self.layout.positions[node]
        x += self.offset_x
        y += self.offset_y
        self.circles.append("""<circle stroke="black" stroke-width="%d" fill="#%02x%02x%02x" cx="%f" cy="%f" r="%f" />""" % (node.height+2, rgb[0]*255,rgb[1]*255,rgb[2]*255, x,y, node.inner_radius))
        
        if len(node.children) != 0 and self.show_bounding:
            bx,by = self.layout.bounding_positions[node]
            bx += self.offset_x
            by += self.offset_y
            self.circles.append("""<circle stroke="blue" stroke-width="%d" fill="none" cx="%f" cy="%f" r="%f" />""" % (1, bx,by, node.bounding_radius))
        
        if node.parent != None:
            px,py = self.layout.positions[node.parent]
            px += self.offset_x
            py += self.offset_y
            self.lines.append("""<line stroke="grey" stroke-width="%f" fill="none" x1="%f" y1="%f" x2="%f" y2="%f" />""" % (node.height+1, px, py, x, y))
        
        if len(node.content) != 0:
            ls = node.content.split('\n')
            ty = y-(6*len(ls)+8)*node.scale
            a = []
            a.append("""<text stroke="none" stroke-width="0" fill="black" x="%f" y="%f" text-anchor="middle" font-size="%f">""" % (x, ty, 12*node.scale))
            ly = 16*node.scale
            for l in ls:
                a.append("""<tspan x="%f" dy="%f">%s</tspan>""" % (x, ly, l))
            a.append("""</text>""")
            self.texts.append(''.join(a))
        
        for c in node.children:
            if parent_hue is None:
                k = 0#random.random()
                new_hue = k + c.theta / (2.0*math.pi)
            self.render_node(c, new_hue)


def main(args=None):
    if args is None:
        args = sys.argv

    #n = tree.random_tree(20)
    #n = tree.life_tree()
    
    filename = args[1]
    n = tree.load(filename)
    
    l = CircleLayout(n)
    l.run()
    r = SvgRenderer(l)
    r.show_bounding = True
    r.render()


if __name__ == '__main__':
    import psyco
    psyco.full()
    
    main()
