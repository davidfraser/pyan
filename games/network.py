import sys
import time
import random
import pygame


class Tile(object):
    def __init__(self, x, y, ports=[False]*4, source=False):
        self.x = x
        self.y = y
        self.ports = ports[:]
        self.source = source
        self.true_ports = ports[:]
        self.locked = False
        self.heat = 0.0
        self.prev_heat = 0.0
        self.possibles = [True]*4
        self.necessaries = [False]*4
        self.dead_end = False
        self.live_end = False
        self.mark = False


    def rotate(self, steps=1):
        self.ports = self.ports[steps:] + self.ports[:steps]
        self.heat = 0.0


class Map(object):
    
    DEFAULT_WIDTH=9
    DEFAULT_HEIGHT=9
    
    def __init__(self, width, height, torus=False, num_seeds=1):
        self.width = self.DEFAULT_WIDTH
        self.height = self.DEFAULT_HEIGHT
        self.torus = False
        self.num_seeds = 1
        self.clear(width, height, torus, num_seeds)


    def clear(self, width=None, height=None, torus=None, num_seeds=None):
        if width is not None:
            self.width = width
        if height is not None:
            self.height = height
        if torus is not None:
            self.torus = torus
        if num_seeds is not None:
            self.num_seeds = num_seeds
        self.tiles = [None]*self.height
        for i in range(self.height):
            self.tiles[i] = [None]*self.width
        self.sinks = 0
        self.lights = 0
        self.locks = 0


    def generate(self):
        seeds = []
        for k in range(self.num_seeds):
            i = random.randint(0, self.height - 1)
            j = random.randint(0, self.height - 1)
            t = Tile(j, i, source=True)
            self.tiles[i][j] = t
            seeds.append(t)
        
        while seeds:
            r = random.randint(0,len(seeds)-1)
            s = seeds[r]
            ns = self.get_neighbours(s)
            if all([n is not None for n in ns]):
                seeds = seeds[:r] + seeds[r+1:]
                continue
            r = random.randint(0,3)
            if ns[r] is not None:
                continue
            x,y = self.get_coords(s.x, s.y, r)
            t = Tile(x, y)
            self.tiles[y][x] = t
            if sum(s.ports) < 3:
                s.ports[r] = True
                t.ports[(r+2)%4] = True
                seeds.append(t)
        
        for row in self.tiles:
            for t in row:
                t.true_ports = t.ports[:]
                t.rotate(random.randint(0,3))
                if sum(t.ports) == 1 and not t.source:
                    self.sinks += 1
                if sum(t.ports) == 0:
                    self.locked = True


    def get_neighbours(self, s):
        ns = [False]*4
        
        if self.torus:
            for i in range(4):
                x,y = self.get_coords(s.x, s.y, i)
                ns[i] = self.tiles[y][x]
            return ns
        
        if s.y > 0:
            ns[0] = self.tiles[s.y-1][s.x]
        if s.x > 0:
            ns[1] = self.tiles[s.y][s.x-1]
        if s.y < self.height-1:
            ns[2] = self.tiles[s.y+1][s.x]
        if s.x < self.width-1:
            ns[3] = self.tiles[s.y][s.x+1]
        return ns


    def get_coords(self, x, y, num):
        if num == 0:
            x,y = x, y-1
        elif num == 1:
            x,y = x-1, y
        elif num == 2:
            x,y = x, y+1
        else:
            x,y = x+1, y
        
        x = x % self.width
        y = y % self.height
        return x, y


    def update(self):
        adj = []
        for i in range(self.height):
            adj.append([-0.01]*self.width)
        
        for i in range(self.height):
            for j in range(self.width):
                if self.tiles[i][j] in [None,False]:
                    continue
                if self.tiles[i][j].source:
                    adj[i][j] = 1 - self.tiles[i][j].heat
                else:
                    ns = self.get_neighbours(self.tiles[i][j])
                    for k in range(4):
                        if ns[k] not in [False,None] and self.tiles[i][j].ports[k] and ns[k].ports[(k + 2) % 4]:
                            d = (ns[k].heat - self.tiles[i][j].heat)/2.0
                            if d < 0:
                                d = 0
                            adj[i][j] += d
        
        changed = False
        self.lights = 0
        self.locks = 0
        for i in range(self.height):
            for j in range(self.width):
                tile = self.tiles[i][j]
                if tile in [None,False]:
                    continue
                tile.prev_heat = tile.heat
                tile.heat += adj[i][j]
                changed2 = adj[i][j] != 0.0
                if tile.heat < 0:
                    tile.heat = 0
                    changed2 = False
                if tile.heat > 1.0:
                    tile.heat = 1.0
                    changed2 = False
                changed |= changed2
                
                if sum(tile.ports) == 1 and not tile.source:
                    if tile.heat > 0.0 and tile.heat >= tile.prev_heat:
                        self.lights += 1
                
                if tile.locked:
                    self.locks += 1
        
        return changed


    def solve(self):
        for row in self.tiles:
            for t in row:
                t.ports = t.true_ports


    def scroll(self, dx, dy):
        dx = (dx + self.width) % self.width
        dy = (dy + self.height) % self.height
        if dy != 0:
            self.tiles = self.tiles[dy:] + self.tiles[:dy]
        if dx != 0:
            for i in range(self.height):
                self.tiles[i] = self.tiles[i][dx:] + self.tiles[i][:dx]
        
        for row in self.tiles:
            for tile in row:
                tile.x = (tile.x - dx) % self.width
                tile.y = (tile.y - dy) % self.height


class Window(object):

    def __init__(self, map, width, height, display):
        self.map = map
        self.width = width
        self.height = height
        self.display = display
        self.font = pygame.font.SysFont(None, 25)
        self.scroll_x = 0
        self.scroll_y = 0


    def scroll(self, dx, dy):
        self.scroll_x += dx
        self.scroll_y += dy
        if not self.map.torus:
            max_x = self.map.width - self.width
            max_y = self.map.height - self.height
            if self.scroll_x < 0:
                self.scroll_x = 0
            if self.scroll_y < 0:
                self.scroll_y = 0
            if self.scroll_x > max_x:
                self.scroll_x = max_x
            if self.scroll_y > max_y:
                self.scroll_y = max_y
        else:
            self.scroll_x = self.scroll_x % self.map.width
            self.scroll_y = self.scroll_y % self.map.height


    def draw(self):
        self.draw_map()

    def draw_tile(self, tile, row, col):
        if tile is None or not any(tile.ports):
            pygame.draw.rect(self.display, (50, 50, 100), pygame.Rect(col*50+1, row*50+1, 48, 48))
            return
        if tile.locked:
            pygame.draw.rect(self.display, (50, 100, 150), pygame.Rect(col*50+1, row*50+1, 48, 48))
        else:
            pygame.draw.rect(self.display, (150, 100, 50), pygame.Rect(col*50+1, row*50+1, 48, 48))
        wire_col = [tile.heat*200, 50, 200-tile.heat*200]
        if tile.heat < tile.prev_heat:
            wire_col[1] = 200
        if tile.ports[0]:
            pygame.draw.rect(self.display, wire_col, pygame.Rect(col*50+21, row*50+1, 8, 24))
        if tile.ports[1]:
            pygame.draw.rect(self.display, wire_col, pygame.Rect(col*50+1, row*50+21, 24, 8))
        if tile.ports[2]:
            pygame.draw.rect(self.display, wire_col, pygame.Rect(col*50+21, row*50+25, 8, 24))
        if tile.ports[3]:
            pygame.draw.rect(self.display, wire_col, pygame.Rect(col*50+25, row*50+21, 24, 8))
        if tile.source:
            pygame.draw.circle(self.display, (200, 150, 50), (col*50+25, row*50+25), 12)
        if any(tile.ports):
            pygame.draw.circle(self.display, wire_col, (col*50+25, row*50+25), 4)
        if sum(tile.ports) == 1 and not tile.source:
            pygame.draw.rect(self.display, (100, 200, 100), pygame.Rect(col*50+11, row*50+11, 28, 28))
            if tile.heat > 0.0 and tile.heat >= tile.prev_heat:
                pygame.draw.rect(self.display, (250, 250, 0), pygame.Rect(col*50+21, row*50+21, 8, 8))
        #text = self.font.render("%0.2f" % tile.heat, False, (255,255,255))
        #self.display.blit(text, (col*50, row*50))
        extra_str = ""
        if tile.mark:
            extra_str = extra_str + 'M'
        if tile.dead_end:
            extra_str = extra_str + 'D'
        if tile.live_end:
            extra_str = extra_str + 'L'
        text = self.font.render(extra_str, False, (255,255,255))
        self.display.blit(text, (col*50, row*50 + 50 - text.get_height()))

    def draw_mini_tile(self, tile, row, col, offset_x, offset_y):
        if tile is None or not any(tile.ports):
            return
        bg_col = (100, 100, 100)
        if (col - self.scroll_x) % self.map.width < self.width and (row - self.scroll_y) % self.map.height < self.height:
            bg_col = (150, 150, 150)
        if tile.mark:
            bg_col = (255, 255, 255)
        pygame.draw.rect(self.display, bg_col, pygame.Rect(col*6+offset_x, row*6+offset_y, 6, 6))
        wire_col = [tile.heat*200, 50, 200-tile.heat*200]
        if tile.heat < tile.prev_heat:
            wire_col[1] = 200
        if tile.ports[0]:
            pygame.draw.rect(self.display, wire_col, pygame.Rect(col*6+offset_x+2, row*6+offset_y, 2,4))
        if tile.ports[1]:
            pygame.draw.rect(self.display, wire_col, pygame.Rect(col*6+offset_x, row*6+offset_y+2, 4,2))
        if tile.ports[2]:
            pygame.draw.rect(self.display, wire_col, pygame.Rect(col*6+offset_x+2, row*6+offset_y+2, 2,4))
        if tile.ports[3]:
            pygame.draw.rect(self.display, wire_col, pygame.Rect(col*6+offset_x+2, row*6+offset_y+2, 4,2))
        if sum(tile.ports) == 1 and not tile.source:
            pygame.draw.rect(self.display, (100, 200, 100), pygame.Rect(col*6+offset_x+2, row*6+offset_y+2, 2,2))
            if tile.heat > 0.0 and tile.heat >= tile.prev_heat:
                pygame.draw.rect(self.display, (250, 250, 0), pygame.Rect(col*6+offset_x+2, row*6+offset_y+2, 2,2))

    def draw_map(self):
        for i in range(self.height):
            for j in range(self.width):
                t = self.map.tiles[(self.scroll_y + i) % self.map.height][(self.scroll_x + j) % self.map.width]
                self.draw_tile(t, i, j)
        
        info_str = '%d locks, %d lights' % (self.map.locks, self.map.lights)
        if self.map.lights == self.map.sinks:
            info_str = info_str + ' , Won!'
        text = self.font.render(info_str, False, (255,255,255))
        self.display.blit(text, (0,0))
            
        offset_x = self.display.get_size()[0] - 6 * self.map.width - 10
        offset_y = 6
        for i in range(self.map.height):
            for j in range(self.map.width):
                self.draw_mini_tile(self.map.tiles[i][j], i, j, offset_x, offset_y)
    
    def locate_click(self, x, y):
        x = x / 50
        y = y / 50
        return (x + self.scroll_x) % self.map.width, (y + self.scroll_y) % self.map.height


solve_stack = []
def solve_one(map, extra=False):
    if len(solve_stack) == 0:
        for i in range(map.height):
            for j in range(map.width):
                solve_stack.append((i, j))
        random.Random().shuffle(solve_stack)
    
    while len(solve_stack) > 0:
        i, j = solve_stack.pop()
        tile = map.tiles[i][j]
        ns = map.get_neighbours(tile)
        
        if tile.source and not tile.live_end:
            tile.live_end = True
        
        if sum(tile.ports) == 1 and not tile.dead_end:
            tile.dead_end = True
        
        # If most connected neighbours are dead ends, so's this.
        if not tile.dead_end:
            not_deads = 0
            for k in range(4):
                if not tile.necessaries[k]:
                    continue
                if not ns[k].dead_end:
                    not_deads += 1
            if not_deads <= 1:
                tile.dead_end = True
        
        # If at least one connected neighbours is a live end, so's this.
        if not tile.live_end:
            lives = 0
            for k in range(4):
                if not tile.necessaries[k]:
                    continue
                if ns[k].live_end:
                    lives += 1
            if lives >= 1:
                tile.live_end = True
        
        if tile.locked or not any(tile.ports):
            tile.possibles = tile.ports
            tile.necessaries = tile.ports
            continue
        if sum(tile.possibles) == sum(tile.ports):
            tile.ports = tile.possibles
            if not tile.locked:
                tile.locked = True
                for n in ns:
                    solve_stack.append((n.y, n.x))
                continue
        if sum(tile.necessaries) == sum(tile.ports):
            tile.ports = tile.necessaries
            if not tile.locked:
                tile.locked = True
                for n in ns:
                    solve_stack.append((n.y, n.x))
                continue
        
        # If all but one of my possible neighbours is dead, then the one remaining
        # one is necessary.
        num_deads = 0
        not_dead = None
        for k in range(4):
            if not tile.possibles[k]:
                continue
            if ns[k].dead_end:
                num_deads += 1
            elif not tile.necessaries[k]:
                not_dead = k
        if not_dead is not None and num_deads == sum(tile.possibles) - 1:
            if extra:
                tile.necessaries[not_dead] = True
                tile.mark = True
                return False
        
        ns = map.get_neighbours(tile)
        for k in range(4):
            # Copy necessary and possible information from neighbours
            if not ns[k].possibles[(k + 2) % 4]:
                tile.possibles[k] = False
            if ns[k].necessaries[(k + 2) % 4]:
                tile.necessaries[k] = True
            if tile.ports in [[True,False,True,False], [False,True,False,True]]:
                # If it's a horizontal or vertical pipe, the opposite of necessary is necessary
                if ns[k].necessaries[(k + 2) % 4]:
                    tile.necessaries[(k + 2) % 4] = True
                # And the opposite of impossible is impossible
                if not ns[k].possibles[(k + 2) % 4]:
                    tile.possibles[(k + 2) % 4] = False
            elif sum(tile.ports) == 2:
                # If it's a corner, the opposite of a necessary is impossible
                if ns[k].necessaries[(k + 2) % 4]:
                    tile.possibles[(k + 2) % 4] = False
                # And the opposite if impossible is necessary
                if not ns[k].possibles[(k + 2) % 4]:
                    tile.necessaries[(k + 2) % 4] = True
            elif sum(tile.ports) == 1:
                # If it's an endpoint and the neighbour is an endpoint, that connection is impossible
                if sum(ns[k].ports) == 1:
                    tile.possibles[k] = False
            
            # If i'm live, and the neighbour not necessary and also live, then
            # it's impossible.
            if tile.live_end and ns[k].live_end and not tile.necessaries[k]:
                tile.possibles[k] = False
            
            # Check if nearest three edges are necessary, if so this is impossible
            if extra:
                neccs = True
                n = tile
                for i in range(3):
                    n = map.get_neighbours(n)[(k+i) % 4]
                    if not n.necessaries[(k+i+1) % 4]:
                        neccs = False
                        break
                if neccs:
                    tile.possibles[k] = False
    
    return True


def main(args=None):
    if args is None:
        args = sys.argv
    
    WINDOW_WIDTH=9
    WINDOW_HEIGHT=9
    
    pygame.init()
    display = pygame.display.set_mode((WINDOW_WIDTH * 50, WINDOW_HEIGHT * 50))
    pygame.time.set_timer(pygame.USEREVENT+1, 250)

    map = Map(20, 20, True, 3)
    window = Window(map, WINDOW_WIDTH, WINDOW_HEIGHT, display)

    map.generate()
    window.draw()
    pygame.display.flip()
    
    auto_solve = False
    extra_solve = False

    while True:
        ev = pygame.event.wait()
        if ev.type == pygame.KEYDOWN:
            if ev.key == pygame.K_ESCAPE:
                break
            elif ev.key == pygame.K_r:
                map.clear()
                map.generate()
                window.draw()
                pygame.display.flip()
            elif ev.key == pygame.K_f:
                map.solve()
                window.draw()
                pygame.display.flip()
            elif ev.key == pygame.K_a:
                auto_solve = not auto_solve
            elif ev.key == pygame.K_x:
                extra_solve = not extra_solve
            elif ev.key == pygame.K_s:
                solve_one(map)
                window.draw()
                pygame.display.flip()
            elif ev.key == pygame.K_UP:
                window.scroll(0,-1)
                window.draw()
                pygame.display.flip()
            elif ev.key == pygame.K_DOWN:
                window.scroll(0,1)
                window.draw()
                pygame.display.flip()
            elif ev.key == pygame.K_LEFT:
                window.scroll(-1,0)
                window.draw()
                pygame.display.flip()
            elif ev.key == pygame.K_RIGHT:
                window.scroll(1,0)
                window.draw()
                pygame.display.flip()
        if ev.type == pygame.MOUSEBUTTONDOWN:
            x, y = ev.pos
            x, y = window.locate_click(x, y)
            if x >= 0 and y >= 0 and x < map.width and y < map.height:
                if ev.button == 1 and not map.tiles[y][x].locked:
                    map.tiles[y][x].rotate(3)
                elif ev.button == 3 and not map.tiles[y][x].locked:
                    map.tiles[y][x].rotate(1)
                elif ev.button == 2:
                    map.tiles[y][x].locked = not map.tiles[y][x].locked
                    if not map.tiles[y][x].locked:
                        map.tiles[y][x].possibles = [True]*4
                        map.tiles[y][x].necessaries = [False]*4
                window.draw()
                pygame.display.flip()
        elif ev.type == pygame.USEREVENT+1:
            if auto_solve:
                auto_solve = solve_one(map, extra_solve)
            if map.update():
                window.draw()
                pygame.display.flip()

    pygame.quit


try:
    import psyco
    psyco.full()
    #print 'Optimised'
except ImportError:
    #print 'Not optimised'
    pass


if __name__ == '__main__':
    main()
