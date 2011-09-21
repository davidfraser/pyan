import sys
import math
import random
import os

import wx
import wx.grid

from layout import CircleLayout
import justify
from renderer import WxLayoutRenderer
from tree_editor import TreeEditor
import tree_editor
import tree


VERSION = '0.1'


PROPERTIES=['hue', 'layout']


class EditNodeDialog(wx.Dialog):
    
    def __init__(
            self, content, properties, parent, id=-1, title="Edit node", size=wx.DefaultSize, pos=wx.DefaultPosition, 
            style=wx.DEFAULT_DIALOG_STYLE|wx.RESIZE_BORDER,
            ):
        
        wx.Dialog.__init__(self, parent, id, title=title, size=size, style=style)
        
        sizer = wx.BoxSizer(wx.VERTICAL)

        label = wx.StaticText(self, -1, "Node content")
        sizer.Add(label, 0, wx.ALL, 5)

        self.content_control = wx.TextCtrl(self, -1, content, size=(80,-1), style=wx.TE_MULTILINE)
        sizer.Add(self.content_control, 1, wx.EXPAND|wx.ALL, 5)

        label = wx.StaticText(self, -1, "Properties")
        sizer.Add(label, 0, wx.ALL, 5)
        
        self.property_grid = wx.grid.Grid(self, -1)
        sizer.Add(self.property_grid, 2, wx.EXPAND|wx.ALL, 5)
        
        self.property_grid.CreateGrid(len(properties)+1,2)
        self.property_grid.SetColLabelValue(0, "Name")
        self.property_grid.SetColLabelValue(1, "Value")
        self.prop_name_editor = wx.grid.GridCellChoiceEditor(PROPERTIES, True)

        i = 0
        for k,v in properties.iteritems():
            self.property_grid.SetCellValue(i, 0, k)
            self.property_grid.SetCellValue(i, 1, v)
            self.property_grid.SetCellEditor(i, 0, self.prop_name_editor)
            i += 1
        
        self.property_grid.SetCellEditor(i, 0, self.prop_name_editor)
        
        btnsizer = wx.StdDialogButtonSizer()
        
        btn = wx.Button(self, wx.ID_OK)
        btn.SetDefault()
        btnsizer.AddButton(btn)

        btn = wx.Button(self, wx.ID_CANCEL)
        btnsizer.AddButton(btn)
        btnsizer.Realize()

        sizer.Add(btnsizer, 0, wx.ALIGN_CENTER|wx.ALIGN_CENTER_VERTICAL|wx.ALL, 5)
        
        self.Bind(wx.grid.EVT_GRID_CELL_CHANGE, self.OnGridCellChange)
        
        self.SetSizer(sizer)
        sizer.Fit(self)
    
    def OnGridCellChange(self, evt):
        if evt.GetRow() >= self.property_grid.GetNumberRows()-1:
            self.property_grid.AppendRows(1)
            self.property_grid.SetCellEditor(evt.GetRow()+1, 0, self.prop_name_editor)
    
    def GetNewContent(self):
        return self.content_control.GetValue()
    
    def GetNewProperties(self):
        props = {}
        for i in range(self.property_grid.GetNumberRows()):
            k = self.property_grid.GetCellValue(i, 0)
            v = self.property_grid.GetCellValue(i, 1)
            if k != '' and v != '':
                props[k] = v
        
        return props


class LayoutPanel(wx.Panel):
    def __init__(self, 
                 parent=None, ID=-1
                 ):

        wx.Panel.__init__(self, parent, size=(1,1), style=wx.WANTS_CHARS)
        
        self.Bind(wx.EVT_MOUSEWHEEL, self.OnMouseScroll)
        self.Bind(wx.EVT_MOTION, self.OnMouseMove)
        self.Bind(wx.EVT_LEFT_UP, self.OnMouseLeftUp)
        self.Bind(wx.EVT_LEFT_DCLICK, self.OnMouseLeftDoubleClick)

        self.Bind(wx.EVT_KEY_DOWN, self.OnKeyDown)

        self.Bind(wx.EVT_ERASE_BACKGROUND, lambda evt: None)
        self.Bind(wx.EVT_PAINT, self.OnPaint)
        self.Bind(wx.EVT_SIZE, lambda e: self.Refresh())
        
        self.drag_pos = None
        
        self.renderer = WxLayoutRenderer()
        self.editor = TreeEditor(self)

    def SetTree(self, tree, filename):
        self.layout = CircleLayout(tree)
        self.layout.get_text_size = get_text_size
        self.layout.run()
        self.renderer.layout = self.layout
        self.filename = filename
        self.dirty = False
        
        self.show_bounding_circles = False
        self.auto_relayout = True
        self.scroll_x = 0
        self.scroll_y = 0
        self.zoom = 1.0
        
        self.selected_node = None
        
        self.ReLayout()
    
    def OnMouseScroll(self, evt):
        if evt.CmdDown():
            r = 1.125**(evt.GetWheelRotation()/120.0)
            self.zoom *= r
            self.scroll_x *= r
            self.scroll_y *= r
            self.Refresh()

    def OnMouseMove(self, evt):
        if not evt.Dragging():
            self.drag_pos = None
        else:
            self.CaptureMouse()
            if not self.drag_pos:
                self.drag_pos = evt.GetPosition()
                self.start_x,self.start_y = self.scroll_x,self.scroll_y
            else:
                displacement = evt.GetPosition() - self.drag_pos
                self.scroll_x = self.start_x + displacement.x
                self.scroll_y = self.start_y + displacement.y
                self.Refresh()
            self.ReleaseMouse()

    def OnMouseLeftUp(self, evt):
        if self.drag_pos is not None:
            self.drag_pos = None
            return
        
        x,y = evt.GetPositionTuple()
        x,y = self.renderer.screen_to_coord(x, y)
        n = self.layout.find_node(x,y)
        self.SelectNode(n)

    def OnMouseLeftDoubleClick(self, evt):
        self.OnMouseLeftUp(evt)
        self.OnEditNode(evt)

    def OnKeyDown(self, evt):
        if evt.CmdDown():
            if evt.GetKeyCode() == wx.WXK_HOME:
                self.SelectNode(self.layout.root)
            elif evt.GetKeyCode() in [wx.WXK_LEFT, wx.WXK_RIGHT]:
                self.MoveNode(evt.GetKeyCode())
            else:
                evt.Skip()
            return

        if evt.GetKeyCode() in [wx.WXK_UP, wx.WXK_DOWN, wx.WXK_LEFT, wx.WXK_RIGHT]:
            self.MoveSelection(evt.GetKeyCode())
        elif evt.GetKeyCode() == wx.WXK_F9:
            self.ReLayout()
        elif evt.GetKeyCode() == wx.WXK_BACK and self.selected_node is not None:
            self.selected_node = self.selected_node.parent  
            self.ReLayout()
        elif evt.GetKeyCode() == ord('B'):
            self.show_bounding_circles = not self.show_bounding_circles
            self.Refresh()
        elif evt.GetKeyCode() == ord('P'):
            self.Pivot()
        elif evt.GetKeyCode() == ord('R'):
            self.SetRoot()
        else:
            evt.Skip()
        
    def OnPaint(self, evt):
        s = self.GetSize()
        dc = wx.MemoryDC()
        dc.SelectObject(wx.EmptyBitmap(s.x, s.y))
        
        dc.SetBrush(wx.WHITE_BRUSH)
        dc.Clear()

        gc = wx.GraphicsContext.Create(dc)
        
        self.renderer.zoom = self.zoom
        self.renderer.scroll_x = self.scroll_x
        self.renderer.scroll_y = self.scroll_y
        self.renderer.selected_node = self.selected_node
        self.renderer.show_bounding  = self.show_bounding_circles
        self.renderer.gc = gc
        self.renderer.dc = dc
        self.renderer.render()
        
        dc2 = wx.PaintDC(self)
        dc2.Blit(0,0,s.x,s.y,dc,0,0)
        del dc2
    
    def MoveSelection(self, key_code):
        if self.selected_node is None:
            return
        
        node = self.selected_node
        all_neighbours = []
        if node.parent is not None:
            all_neighbours.append(node.parent)
        all_neighbours.extend(node.children)
        if node.parent is not None:
            all_neighbours.extend([c for c in node.parent.children if c != node])
        
        relevant_neighbours = []
        x,y = self.layout.positions[node]
        for n in all_neighbours:
            nx,ny = self.layout.positions[n]
            dx = nx - x
            dy = ny - y
            if ((-dx > abs(dy) and key_code == wx.WXK_LEFT)
                    or (dx > abs(dy) and key_code == wx.WXK_RIGHT)
                    or (-dy > abs(dx) and key_code == wx.WXK_UP)
                    or (dy > abs(dx) and key_code == wx.WXK_DOWN)):
                relevant_neighbours.append(n)
        
        if len(relevant_neighbours) > 0:
            new_node = relevant_neighbours[0]
            self.SelectNode(new_node)

    def SelectNode(self, node):
        if node != self.selected_node:
            self.selected_node = node
            self.Refresh()

    def Pivot(self):
        if self.selected_node is None or self.selected_node.parent is None:
            return
        mutation = tree_editor.PivotMutation(self.layout, self.selected_node)
        self.selected_node = mutation.parent
        self.editor.perform(mutation)
        self.dirty = True

    def SetRoot(self):
        if self.selected_node is None or self.selected_node.parent is None:
            return
        
        mutation = tree_editor.SetRootMutation(self.layout, self.selected_node)
        self.selected_node = mutation.old_root
        self.editor.perform(mutation)
        self.dirty = True

    def MoveNode(self, key_code):
        if self.selected_node is None or self.selected_node.parent is None:
            return
        if key_code == wx.WXK_LEFT and self.selected_node != self.selected_node.parent.children[0]:
            direction = -1
        elif key_code == wx.WXK_RIGHT and self.selected_node != self.selected_node.parent.children[-1]:
            direction = 1
        else:
            return
        mutation = tree_editor.MoveMutation(self.layout, self.selected_node, direction)
        self.editor.perform(mutation)
        self.dirty = True

    def OnZoomAll(self, evt):
        diameter = self.layout.root.bounding_radius*2
        self.scroll_x,self.scroll_y = 0,0
        self.zoom = min(self.renderer.width, self.renderer.height)/float(diameter)
        self.Refresh()

    def OnUndo(self, evt):
        self.editor.undo()
    
    def OnRedo(self, evt):
        self.editor.redo()
    
    def OnCopy(self, evt):
        if self.selected_node is None:
            return
        target = self.selected_node
        
        node_text = tree.to_string(target)

        clipdata = wx.TextDataObject()
        clipdata.SetText(node_text)
        wx.TheClipboard.Open()
        wx.TheClipboard.SetData(clipdata)
        wx.TheClipboard.Close()
    
    def OnCut(self, evt):
        if self.selected_node is None:
            return
        
        self.OnCopy(evt)
        self.OnDeleteNode(evt)

    def OnPaste(self, evt):
        target = self.selected_node
        if not target:
            target = self.layout.root
        
        if wx.TheClipboard.IsOpened():
            return
    
        do = wx.TextDataObject()
        wx.TheClipboard.Open()
        success = wx.TheClipboard.GetData(do)
        wx.TheClipboard.Close()
        if not success:
            return
        
        node = tree.from_string(do.GetText())
        
        mutation = tree_editor.InsertMutation(self.layout, target, node)
        self.editor.perform(mutation)
        self.dirty = True
    
    def OnDeleteNode(self, evt):
        if self.selected_node is not None and self.selected_node != self.layout.root:
            mutation = tree_editor.DeleteMutation(self.layout, self.selected_node)
            self.selected_node = None
            self.editor.perform(mutation)
            self.dirty = True

    def OnEditNode(self, evt):
        if self.selected_node is None:
            return
            
        #new_text = wx.GetTextFromUser("Edit node content", "Edit node", self.selected_node.content.replace('\n', ' '))
        dlg = EditNodeDialog(self.selected_node.content, self.selected_node.properties, self.GetParent())
        dlg.Show()
        val = dlg.ShowModal()
        new_text = dlg.GetNewContent()
        new_props = dlg.GetNewProperties()
        dlg.Destroy()
        
        if val != wx.ID_OK:
            return
        
        if len(new_text) == 0:
            return
        
        points = justify.get_points(new_text)
        all_js = justify.justify_text(points, 2)
        j = all_js[0][1]
        new_text = justify.render_text(new_text, j)
        
        mutation = tree_editor.EditMutation(self.layout, self.selected_node, new_text, new_props)
        self.editor.perform(mutation)
        self.dirty = True

    def OnAddNode(self, evt):
        if self.selected_node is None:
            return
        
        new_text = wx.GetTextFromUser("Enter new node content", "New node", '')
        if len(new_text) == 0:
            return
            
        points = justify.get_points(new_text)
        all_js = justify.justify_text(points, 2)
        j = all_js[0][1]
        new_text = justify.render_text(new_text, j)
        
        n = tree.Node(new_text)
        mutation = tree_editor.InsertMutation(self.layout, self.selected_node, n)
        self.editor.perform(mutation)
        self.dirty = True
        
        wx.PostEvent(self, evt)

    def ReLayout(self):
        self.layout.run()
        self.Refresh()


class MainFrame(wx.Frame):
    def __init__(self, 
                 parent=None, ID=-1, pos=wx.DefaultPosition,
                 size=wx.Size(800,600), style=wx.DEFAULT_FRAME_STYLE
                 ):

        title = "Snowflake"
        wx.Frame.__init__(self, parent, ID, title, pos, size, style)
        
        self.panel = LayoutPanel(self)
        
        # Menu bar
        menu_bar = wx.MenuBar()

        file_menu = wx.Menu()
        file_menu.Append(wx.ID_NEW, "&New\tCtrl-N", "Create a new tree")
        file_menu.Append(wx.ID_OPEN, "&Open...\tCtrl-O", "Open a tree")
        file_menu.Append(wx.ID_SAVE, "&Save\tCtrl-S", "Save this tree")
        file_menu.Append(wx.ID_SAVEAS, "Save &As...", "Save this tree under a different filename")
        file_menu.AppendSeparator()
        import_menu = wx.Menu()
        freemind_id = wx.NewId()
        import_menu.Append(freemind_id, "&FreeMind...", "Import a FreeMind mindmap tree")
        file_menu.AppendMenu(wx.NewId(), "&Import", import_menu)
        file_menu.AppendSeparator()
        file_menu.Append(wx.ID_EXIT, "E&xit\tAlt-F4", "Exit this program")
        menu_bar.Append(file_menu, "&File")
        self.Bind(wx.EVT_MENU, self.OnFileNew, id=wx.ID_NEW)
        self.Bind(wx.EVT_MENU, self.OnFileOpen, id=wx.ID_OPEN)
        self.Bind(wx.EVT_MENU, self.OnFileSave, id=wx.ID_SAVE)
        self.Bind(wx.EVT_MENU, self.OnFileSaveAs, id=wx.ID_SAVEAS)
        self.Bind(wx.EVT_MENU, self.OnImportFreeMind, id=freemind_id)
        self.Bind(wx.EVT_MENU, self.OnFileExit, id=wx.ID_EXIT)
        
        edit_menu = wx.Menu()
        edit_menu.Append(wx.ID_UNDO, "&Undo\tCtrl-Z", "Undo the last edit operation")
        edit_menu.Append(wx.ID_REDO, "&Redo\tCtrl-Y", "Redo the last edit operation")
        edit_menu.AppendSeparator()
        edit_menu.Append(wx.ID_CUT, "Cu&t\tCtrl-X", "Cut the selected node from the tree")
        edit_menu.Append(wx.ID_COPY, "&Copy\tCtrl-C", "Copy the selected node")
        edit_menu.Append(wx.ID_PASTE, "&Paste\tCtrl-V", "Paste a child into the selected node")
        edit_menu.AppendSeparator()
        add_node_id = wx.NewId()
        edit_menu.Append(add_node_id, "&Add node\tInsert", "Add a new child to the selected node")
        edit_node_id = wx.NewId()
        edit_menu.Append(edit_node_id, "&Edit node\tF2", "Edit the selected node's text or properties")
        delete_node_id = wx.NewId()
        edit_menu.Append(delete_node_id, "&Delete node\tDelete", "Delete the selected node")
        menu_bar.Append(edit_menu, "&Edit")
        self.Bind(wx.EVT_MENU, self.panel.OnUndo, id=wx.ID_UNDO)
        self.Bind(wx.EVT_MENU, self.panel.OnRedo, id=wx.ID_REDO)
        self.Bind(wx.EVT_MENU, self.panel.OnCut, id=wx.ID_CUT)
        self.Bind(wx.EVT_MENU, self.panel.OnCopy, id=wx.ID_COPY)
        self.Bind(wx.EVT_MENU, self.panel.OnPaste, id=wx.ID_PASTE)
        self.Bind(wx.EVT_MENU, self.panel.OnAddNode, id=add_node_id)
        self.Bind(wx.EVT_MENU, self.panel.OnEditNode, id=edit_node_id)
        self.Bind(wx.EVT_MENU, self.panel.OnDeleteNode, id=delete_node_id)
        
        view_menu = wx.Menu()
        view_menu.Append(wx.ID_ZOOM_FIT, "&Zoom all\tZ", "Zoom to show the entire tree")
        menu_bar.Append(view_menu, "&View")
        self.Bind(wx.EVT_MENU, self.panel.OnZoomAll, id=wx.ID_ZOOM_FIT)
        
        help_menu = wx.Menu()
        help_menu.Append(wx.ID_ABOUT, "&About...", "About this program")
        menu_bar.Append(help_menu, "&Help")
        self.Bind(wx.EVT_MENU, self.OnHelpAbout, id=wx.ID_ABOUT)
        
        self.Bind(wx.EVT_CLOSE, self.OnClose)

        self.SetMenuBar(menu_bar)
        
        # Status bar
        self.CreateStatusBar()
        self.SetStatusText("Snowflake Tree Editor")

    def OnFileNew(self, event):
        if not self.CheckUnsaved(event):
            return
        
        t = tree.Node("Start")
        self.panel.SetTree(t, None)
        self.SetTitle("Snowflake")

    def OnFileOpen(self, event):
        if not self.CheckUnsaved(event):
            return
        
        wildcard = "Tree text file|*.txt"
        
        dlg = wx.FileDialog(
            self, message="Choose a file...", defaultDir=os.getcwd(), 
            defaultFile="", wildcard=wildcard, style=wx.OPEN | wx.CHANGE_DIR
            )
        
        if dlg.ShowModal() == wx.ID_OK:
            paths = dlg.GetPaths()
            filename = paths[0]
            t = tree.load(filename)
            self.panel.SetTree(t, filename)
            self.SetTitle("Snowflake - " + self.panel.filename)

        dlg.Destroy()

    def OnFileSave(self, event):
        if self.panel.filename is None:
            self.OnFileSaveAs(event)
            return
        
        tree.save(self.panel.layout.root, self.panel.filename)
        self.panel.dirty = False

    def OnFileSaveAs(self, event):
        wildcard = "Tree text file|*.txt"
        
        if self.panel.filename is None:
            filename = ""
        else:
            filename = self.panel.filename
        
        dlg = wx.FileDialog(
            self, message="Save file as ...", defaultDir=os.getcwd(), 
            defaultFile="", wildcard=wildcard, style=wx.SAVE
            )
    
        if dlg.ShowModal() == wx.ID_OK:
            paths = dlg.GetPaths()
            filename = paths[0]
            self.panel.filename = filename
            tree.save(self.panel.layout.root, self.panel.filename)
            self.panel.dirty = False
            self.SetTitle("Snowflake - " + self.panel.filename)
        
        dlg.Destroy()

    def OnClose(self, event):
        if not self.CheckUnsaved(event):
            event.Veto()
            return
        
        self.Destroy()
        
    def OnImportFreeMind(self, event):
        if not self.CheckUnsaved(event):
            return
        
        wildcard = "FreeMind mindmap|*.mm"
        
        dlg = wx.FileDialog(
            self, message="Choose a file...", defaultDir=os.getcwd(), 
            defaultFile="", wildcard=wildcard, style=wx.OPEN | wx.CHANGE_DIR
            )
        
        if dlg.ShowModal() == wx.ID_OK:
            paths = dlg.GetPaths()
            filename = paths[0]
            t = tree.load(filename)
            self.panel.SetTree(t, None)
            self.SetTitle("Snowflake")

        dlg.Destroy()

    def OnHelpAbout(self, event):
        info = wx.AboutDialogInfo()
        info.Name = 'Snowflake Tree Editor'
        info.Version = VERSION
        info.Copyright = 'Copyright (C) 2010, Edmund Horner'
        info.Developers = ['Edmund Horner']
        info.Description = 'An editor for simple trees, such as taxonomies or mindmaps.'
        info.WebSite = 'http://homepages.paradise.net.nz/~ejrh/'
        info.License = 'TBD'
        wx.AboutBox(info)

    def OnFileExit(self, event):
        if not self.CheckUnsaved(event):
            return
        
        self.panel.dirty = False
        self.Close()

    def CheckUnsaved(self, event):
        if not self.panel.dirty:
            return True
            
        answer = wx.MessageBox("Save unsaved changes?", "Unsaved changes",
                        wx.YES_NO | wx.CANCEL, self)
        if answer == wx.CANCEL:
            return False
        elif answer == wx.NO:
            return True
        
        self.OnFileSave(event)
        return not self.panel.dirty

get_text_size_dc = None
get_text_size_font = None

def get_text_size(t, sc):
    global get_text_size_dc, get_text_size_font
    if get_text_size_dc is None:
        get_text_size_dc = wx.MemoryDC()
        get_text_size_font = wx.Font(12, wx.FONTFAMILY_SWISS, wx.FONTSTYLE_NORMAL, wx.FONTWEIGHT_NORMAL)
        
    get_text_size_dc.SetFont(get_text_size_font)
    
    lines = t.split('\n')
    maxw = 0
    maxh = 0
    for l in lines:
        w,h = get_text_size_dc.GetTextExtent(l)
        if w > maxw:
            maxw = w
        maxh += h
    
    hyp = math.sqrt(maxw*maxw + maxh*maxh)
    
    return hyp*sc


def main():
    t = tree.Node("Start")
    app = wx.PySimpleApp()
    frame = MainFrame()
    frame.panel.SetTree(t, None)
    frame.Show(True)
    app.MainLoop()


if __name__ == "__main__":
    try:
        import psyco
        psyco.full()
    except ImportError:
        pass

    main()
