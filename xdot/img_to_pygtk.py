#!/usr/bin/python
#
# Written in 2011 by Bjarni R. Einarsson <http://bre.klaki.net/>
# This script is in the Public Domain.
#
# This is a simple tool for converting image files to compact PyGTK code for
# embedding in Python scripts.  It can read whatever formats PyGTK can.
#
# For a more general-purpose Python embedding tool, check out PyBreeder:
#  - http://pagekite.net/wiki/Floss/PyBreeder/
#
import base64, gobject, gtk, sys, zlib

def image_to_code(name, fn):
  i = gtk.Image()
  i.set_from_file(fn)
  pb = i.get_pixbuf()
  pb64 = base64.b64encode(zlib.compress(pb.get_pixels(), 9))
  data = ''
  while len(pb64) > 0:
    data += "'%s'\n" % pb64[0:78]
    pb64 = pb64[78:]
  return ('%s = gtk.gdk.pixbuf_new_from_data(%s)'
          ) % (name, ', '.join([str(p) for p in [
                                   'zlib.decompress(base64.b64decode(\n%s\n    ))' % data[:-1],
                                   'gtk.gdk.COLORSPACE_RGB', pb.get_has_alpha(),
                                   pb.get_bits_per_sample(), pb.get_width(),
                                   pb.get_height(), pb.get_rowstride()]]))

if __name__ == "__main__":
  if len(sys.argv) > 1:
    args = sys.argv[1:]
    args.append('PIXBUF')
    print image_to_code(args[1], args[0])  
    print
    print '###   Shameless plug: https://pagekite.net/ is awesome! :-)  ###'
    print
  else:
    print 'Usage: %s file.png [variable-name]' % sys.argv[0]
    sys.exit(1)

