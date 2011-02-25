

class Mutation(object):
    pass


class PivotMutation(Mutation):
    def __init__(self, layout, node, pos=-1):
        self.layout = layout
        self.node = node
        self.parent = node.parent
        self.pos = pos
        self.old_pos = node.parent.get_child_position(node)

    def apply(self):
        self.node.pivot(self.pos)
        if self.parent == self.layout.root:
            self.layout.root = self.node

    def inverse(self):
        reversal = PivotMutation(self.layout, self.parent, self.old_pos)
        return reversal


class SetRootMutation(Mutation):
    def __init__(self, layout, node, positions=[]):
        self.layout = layout
        self.node = node
        self.old_root = layout.root
        self.positions = positions
        
        self.old_positions=[]
        while node.parent is not None:
            pos = node.parent.get_child_position(node)-1
            self.old_positions.append(pos)
            node = node.parent

    def apply(self):
        self.layout.root = self.node
        
        node = self.node
        targets = []
        while node.parent is not None:
            targets.append(node)
            node = node.parent
        
        while len(targets) != 0:
            node = targets.pop()
            try:
                pos = self.positions.pop()
            except IndexError:
                pos = -1
            node.pivot(pos)

    def inverse(self):
        reversal = SetRootMutation(self.layout, self.old_root, self.old_positions)
        return reversal


class MoveMutation(Mutation):
    def __init__(self, layout, node, direction):
        self.layout = layout
        self.node = node
        self.direction = direction

    def apply(self):
        self.node.parent.move_child(self.node, self.direction)

    def inverse(self):
        reversal = MoveMutation(self.layout, self.node, -self.direction)
        return reversal


class DeleteMutation(Mutation):
    def __init__(self, layout, node):
        self.layout = layout
        self.node = node
        self.parent = node.parent

    def apply(self):
        self.parent.remove_child(self.node)

    def inverse(self):
        reversal = InsertMutation(self.layout, self.parent, self.node)
        return reversal


class InsertMutation(Mutation):
    def __init__(self, layout, node, child):
        self.layout = layout
        self.node = node
        self.child = child

    def apply(self):
        self.node.add_child(self.child)

    def inverse(self):
        reversal = DeleteMutation(self.layout, self.child)
        return reversal


class EditMutation(Mutation):
    def __init__(self, layout, node, new_text, new_props):
        self.layout = layout
        self.node = node
        self.new_text = new_text
        self.new_props = new_props
        self.old_text = self.node.content
        self.old_props = self.node.properties

    def apply(self):
        self.node.content = self.new_text
        self.node.properties = self.new_props

    def inverse(self):
        reversal = EditMutation(self.layout, self.node, self.old_text, self.old_props)
        return reversal


class TreeEditor(object):
    def __init__(self, panel):
        self.panel = panel
        
        self.undo_stack = []
        self.redo_stack = []
    
    def perform(self, mutation):
        self.undo_stack.append(mutation)
        self.redo_stack = []
        mutation.apply()
        
        self.panel.layout.run()
        self.panel.Refresh()

    def undo(self):
        if len(self.undo_stack) == 0:
            return
        
        mutation = self.undo_stack.pop()
        self.redo_stack.append(mutation)
        reversal = mutation.inverse()
        reversal.apply()
        
        self.panel.layout.run()
        self.panel.Refresh()

    def redo(self):
        if len(self.redo_stack) == 0:
            return
        
        mutation = self.redo_stack.pop()
        self.undo_stack.append(mutation)
        mutation.apply()
        
        self.panel.layout.run()
        self.panel.Refresh()
