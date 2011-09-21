import re
import random
import StringIO


class Node(object):
    def __init__(self, content='', properties=None):
        self.children = []
        self.content = content
        self.height = 0
        self.parent = None
        self.properties = {}
        if properties is not None:
            self.properties.update(properties)

    def add_child(self, child, pos=-1):
        if pos == -1:
            self.children.append(child)
        else:
            self.children.insert(pos, child)
        child.parent = self
        self.bump_height(child.height)
        return child

    def remove_child(self, child):
        child.parent = None
        self.children.remove(child)
        self.height = 0
        for c in self.children:
            self.bump_height(c.height)
    
    def bump_height(self, h):
        if h >= self.height:
            self.height = h+1
        if self.parent is not None:
            self.parent.bump_height(h+1)

    def move_child(self, child, direction):
        ix = self.children.index(child)
        if direction < 0:
            buddy = self.children[ix-1]
            self.children = self.children[:ix-1] + [child, buddy] + self.children[ix+1:]
        elif direction > 0:
            buddy = self.children[ix+1]
            self.children = self.children[:ix] + [buddy, child] + self.children[ix+2:]

    def pivot(self, new_pos=-1):
        """Invert the relationship between this node and its parent."""
        p = self.parent
        gp = p.parent
        if gp is not None:
            ppos = gp.get_child_position(p)
            gp.remove_child(p)
        p.remove_child(self)
        self.add_child(p, new_pos)
        if gp is not None:
            gp.add_child(self, ppos)

    def get_child_position(self, child):
        return self.children.index(child)

    def get_all_descendants(self):
        for c in self.children:
            yield c
            for n in c.get_all_descendants():
                yield n

    def get_all_leaves(self):
        if len(self.children) == 0:
            yield self
            return
        
        for c in self.children:
            for n in c.get_all_leaves():
                yield n


def random_tree(num):
    nodes = []
    for i in range(num):
        n = Node(str(i))
        if i > 0:
            p = nodes[random.randint(0, i-1)]
            p.add_child(n)
        nodes.append(n)
    return nodes[0]


def life_tree():
    life = Node("Living Things")
    a = life.add_child(Node("Animals"))
    v = a.add_child(Node("Vertebrates"))
    
    bird = v.add_child(Node("Birds"))
    kiwi = bird.add_child(Node("Kiwis"))
    eagle = bird.add_child(Node("Eagles"))
    duck = bird.add_child(Node("Ducks"))
    
    inv = a.add_child(Node("Invertebrates"))
    m = v.add_child(Node("Mammals"))
    r = v.add_child(Node("Reptiles"))
    amp = v.add_child(Node("Amphibians"))
    f = v.add_child(Node("Fish"))
    liz = r.add_child(Node("Lizards"))
    shark = f.add_child(Node("Sharks"))
    seah = f.add_child(Node("Seahorses"))
    frog = amp.add_child(Node("Frogs"))
    toad = amp.add_child(Node("Toads"))
    
    snak = r.add_child(Node("Snakes"))
    asp = snak.add_child(Node("Asps"))
    cobra = snak.add_child(Node("Cobras"))
    ana = snak.add_child(Node("Anacondas"))
    python = snak.add_child(Node("Pythons"))
    adder = snak.add_child(Node("Adders"))
    
    crust = inv.add_child(Node("Crustaceans"))
    crab = crust.add_child(Node("Crabs"))
    lobs = crust.add_child(Node("Lobsters"))
    
    sp = inv.add_child(Node("Spiders"))
    kat = sp.add_child(Node("Katipos"))
    
    mol = inv.add_child(Node("Molluscs"))
    oct = mol.add_child(Node("Octopuses"))
    sn = mol.add_child(Node("Snails"))
    
    ins = inv.add_child(Node("Insects"))
    bees = ins.add_child(Node("Bees"))
    beet = ins.add_child(Node("Beetles"))
    fl = ins.add_child(Node("Flies"))
    wa = ins.add_child(Node("Wasps"))
    moth = ins.add_child(Node("Moths"))
    ant = ins.add_child(Node("Ants"))
    
    carn = m.add_child(Node("Carnivores"))
    cat = carn.add_child(Node("Cats"))
    tig = cat.add_child(Node("Tigers"))
    lion = cat.add_child(Node("Lions"))
    chee = cat.add_child(Node("Cheeters"))
    
    dog = carn.add_child(Node("Dogs"))
    wolf = dog.add_child(Node("Wolves"))
    fox = dog.add_child(Node("Foxes"))
    
    otter = carn.add_child(Node("Otters"))
    por = carn.add_child(Node("Porpoises"))
    walr = carn.add_child(Node("Walruses"))
    
    sloth = m.add_child(Node("Sloths"))
    horse = m.add_child(Node("Horses"))
    hippo = m.add_child(Node("Hippopotamuses"))
    plat = m.add_child(Node("Platypuses"))
    wha = m.add_child(Node("Whales"))
    kang = m.add_child(Node("Kangaroos"))
    koala = m.add_child(Node("Koalas"))

    prim = m.add_child(Node("Primates"))
    human = prim.add_child(Node("Human Beans"))
    orang = prim.add_child(Node("Orangutans"))
    gor = prim.add_child(Node("Gorillas"))
    monk = prim.add_child(Node("Monkeys"))

    plant = life.add_child(Node("Plants"))
    seaw = plant.add_child(Node("Seaweeds"))
    grass = plant.add_child(Node("Grasses"))
    bamboo = grass.add_child(Node("Bamboos"))
    tree = plant.add_child(Node("Trees"))
    pump = tree.add_child(Node("Pumpkins"))
    banana = tree.add_child(Node("Bananas"))
    bean = tree.add_child(Node("Actual Beans"))
    
    fung = life.add_child(Node("Funguses"))
    mush = fung.add_child(Node("Mushrooms"))
    tst = fung.add_child(Node("Toadstools"))

    return life


def make_tree_from_mindmap(filename):
    f = open(filename, 'rt')
    node = None
    stack = []
    for l in f:
        l = l.strip()
        if l.startswith('<node'):
            t = re.match('<node .*TEXT="([^"]+)".*>.*', l, re.M).groups(1)[0]
            #print >>sys.stderr, 'start', t
            
            try:
                t = t.replace('&#xa;', '\n')
                while '  ' in t:
                    t = t.replace('  ', ' ')
            except:
                print >> sys.stderr, 'Failed processing text """%s""" from line """%s"""' % (t, l)
                raise
            
            child = Node(t)
            if node is None:
                node = child
                stack = [node]
            else:
                node.add_child(child)
            if not l.endswith('/>'):
                stack.append(node)
                node = child
        elif l.startswith('</node'):
            #print >>sys.stderr, 'stop', node.content
            node = stack.pop()
    f.close()
    
    return node


SEPARATOR = '|'


def to_stream(t, f, indent=0):
    ps = ''
    if len(t.properties) != 0:
        ps = SEPARATOR + ' '.join(k+'='+v for k,v in t.properties.iteritems())
    print >>f, '    '*indent + t.content.replace('\n', ' ') + ps
    for c in t.children:
        to_stream(c, f, indent+1)


def from_stream(f):
    node = None
    stack = []
    corrupt = False
    for l in f:
        l = l.strip('\n')
        t = l
        indent = 0
        while t.startswith('    '):
            indent += 1
            t = t[4:]
        
        props = {}
        if SEPARATOR in t:
            i = t.index(SEPARATOR)
            pt = t[i+1:]
            t = t[:i]
            for p in pt.split(' '):
                try:
                    k,v = p.split('=')
                    props[k] = v
                except ValueError:
                    corrupt = True
        
        #print >>sys.stderr, indent, t
        n = Node(t.strip(), props)
        if node is None:
            #print >>sys.stderr, 'root'
            if indent != 0:
                raise Exception, 'Too much indentation!'
            node = n
            node_indent = indent
            stack = [node]
            root = node
            continue
        
        if indent > node_indent+1:
            raise Exception, 'Too much indentation!'
        
        while indent <= node_indent:
            node = stack.pop()
            node_indent -= 1
        
        node.add_child(n)
        stack.append(node)
        node = n
        node_indent = indent
    
    #todo warn on corruption
    return root


def to_string(t):
    sio = StringIO.StringIO()
    to_stream(t, sio)
    node_text = sio.getvalue()
    return node_text


def from_string(s):
    sio = StringIO.StringIO(s)
    t = from_stream(sio)
    return t


def load(filename):
    """Load a tree from a text file, or a FreeMind mindmap if the filename ends with .mm"""
    if filename.endswith('mm'):
        return make_tree_from_mindmap(filename)
    f = open(filename, 'rt')
    t = from_stream(f)
    f.close()
    return t


def save(t, filename):
    """Save a tree to a text file."""
    f = open(filename, 'wt')
    to_stream(t, f)
    f.close()
