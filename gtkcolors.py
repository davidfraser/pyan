#!/usr/bin/python
#
# http://lobais.blogspot.fi/2006/07/system-colors-in-gtk.html

import pygtk
pygtk.require("2.0")
import gtk, pango

w = gtk.Button()

states = [m for m in dir(gtk) if m.startswith("STATE_")]
styles = [m for m in dir(w.get_style()) if type(getattr(w.get_style(), m)) == type(w.get_style().light)]
styles = [m for m in styles if not m.endswith("_gc")]

class CairoBoard(gtk.DrawingArea):
    def __init__(self):
        gtk.DrawingArea.__init__(self)
        self.connect("expose_event", self.expose)

    def expose(self, widget, event):
        context = widget.window.cairo_create()
        context.rectangle(event.area.x, event.area.y,
                          event.area.width, event.area.height)
        context.clip()
        self.draw(context)
        return False
    
    def draw (self, context):
        for x in range(len(states)):
            for y in range(len(styles)):
                style = getattr(self.get_style(), styles[y])
                state = getattr(gtk, states[x])
                
                color = style[state]
                if color == None or type(color) != gtk.gdk.Color:
                    color = gtk.gdk.Color(0,0,0)
                context.set_source_color(color)
                context.rectangle(x*150,y*50,150,50)
                
                context.fill_preserve()
                context.new_path()
                
                if color.red + color.green + color.blue < 65535:
                    context.set_source_color(gtk.gdk.Color(65535,65535,65535))
                else: context.set_source_color(gtk.gdk.Color(0,0,0))
                
                cstring = "#"
                for col in color.red, color.green, color.blue:
                    h = hex(col/256)[2:]
                    if len(h) == 1:
                        h = "0"+h
                    cstring += h
                    
                layout = self.create_pango_layout ("%s\n%s\n%s" % \
                        (styles[y], states[x][6:], cstring))
                layout.set_font_description(pango.FontDescription("Sans Serif 10"))
                context.move_to(x*150,y*50)
                context.show_layout(layout)

                context.fill_preserve()
                context.new_path()

window = gtk.Window()
window.connect("destroy", gtk.main_quit)
window.add(CairoBoard())
window.set_default_size(len(states)*150, len(styles)*50)
window.show_all()
gtk.main()
