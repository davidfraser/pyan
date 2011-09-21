from random import randrange, random, randint, shuffle
from math import ceil
from time import clock

    
def valuerange():
    return range(3, 16)

JOKER_VALUE = 50

class Card(object):
    def __init__(self, value, suit):
        self.value = value
        self.suit = suit
    
    def equals(self, other):
        return self.value == other.value
    
    def outranks(self, other):
        return self.value > other.value
    
    def __str__(self):
        if self.value >= 3 and self.value <= 10:
            v = '%s' % self.value
        elif self.value == 11:
            v = 'J'
        elif self.value == 12:
            v = 'Q'
        elif self.value == 13:
            v = 'K'
        elif self.value == 14:
            v = 'A'
        elif self.value == 15:
            v = '2'
        elif self.value >= 16:
            return 'Joker'
        
        return '%s%s' % (v, self.suit)

    def __eq__(self, other):
        return (self.suit,self.value) == (other.suit,other.value)

    def __cmp__(self, other):
        if self.outranks(other):
            return 1
        elif other.outranks(self):
            return -1
        else:
            return 0

    def __ne__(self, other):
        return self.suit,self.value != other.suit,other.value

    def __hash__(self):
        return hash(self.__str__())


class Deck(object):
    def __init__(self, cards = None):
        if cards == None:
            cards = []
        self.cards = list(cards)
    
    def clone(self):
        d = Deck()
        d.cards = list(self.cards)
        return d
    
    def full_pack(self, n):
        self.cards = []
        
        for i in range(n):
            for suit in ['S','H','C','D']:
                for value in valuerange():
                    c = Card(value, suit)
                    self.cards.append(c)
            c = Card(JOKER_VALUE, 'J')
            self.cards.append(c)
            c = Card(JOKER_VALUE, 'J')
            self.cards.append(c)
    
    def add(self, card):
        self.cards.append(card)
    
    def add_all(self, deck):
        self.cards.extend(deck.cards)
    
    def remove(self, card):
        self.cards.remove(card)
    
    def remove_all(self, deck):
        for c in deck.cards:
            self.remove(c)
    
    def contains(self, card):
        return card in self.cards
    
    def contains_all(self, cards):
        return all(self.contains(c) for c in cards)
    
    def shuffle(self):
        r = []
        for c in self.cards:
            n = randrange(0, len(r)+1)
            r.insert(n, c)
        return r
    
    def is_empty(self):
        return self.cards == []
    
    def first_card(self):
        for c in self.cards:
            return c
        raise IndexError
    
    def __str__(self):
        return '[' + ' '.join([c.__str__() for c in sorted(self.cards)]) + ']'

    def __cmp__(self, other):
        if self.is_empty() and not other.is_empty():
            return -1
        if not self.is_empty() and other.is_empty():
            return 1
        if self.is_empty() and other.is_empty():
            return 0
        
        c = cmp(self.cards[0], other.cards[0])
        if c != 0:
            return c
        return cmp(len(self.cards), len(other.cards))

    def __eq__(self, other):
        return self.contains_all(other.cards) and other.contains_all(self.cards)

    def __len__(self):
        return len(self.cards)


class Hand(Deck):
    def __init__(self, cards=None):
        super(Hand, self).__init__()
        self.sets = {}
        if cards is not None:
            for c in cards:
                self.add(c)
    
    def clone(self):
        h = Hand(self.cards)
        for k,s in self.sets.iteritems():
            h.sets[k] = s.clone()
        return h
    
    def moves(self):
        m = [Move([])]
        
        for v in self.sets:
            if v == JOKER_VALUE:
                continue
            s = self.sets[v]
            for i in range(1, len(s.cards)+1):
                m.append(Move(list(s.cards)[0:i]))
        
        try:
            if not self.sets[JOKER_VALUE].is_empty():
                m.append(Move(list(self.sets[JOKER_VALUE].cards)[0:1]))
        except:
            pass
            
        return m
    
    def add(self, card):
        super(Hand, self).add(card)
        try:
            self.sets[card.value].add(card)
        except:
            self.sets[card.value] = Deck()
            self.sets[card.value].add(card)
    
    def add_all(self, deck):
        for c in deck.cards:
            self.add(c)

    def remove(self, card):
        super(Hand, self).remove(card)
        self.sets[card.value].remove(card)
        
        if self.sets[card.value].is_empty():
            del self.sets[card.value]
    
    
    def empty(self):
        self.cards = []
        self.sets = {}


    def total_value(self):
        value = 0
        last_value = 0
        for c in self.cards:
            value += c.value - 20
            if c.value != last_value:
                value -= 10
                last_value = c.value
        #print 'value',self,value
        return value


class Move(Deck):
    def __init__(self, cards):
        super(Move, self).__init__(cards)


    def equivalence_class(self):
        if self.is_empty():
            return ''
        if self.cards[0].value == 'J':
            return 'J'
        return '%s*%s' % (len(self.cards), self.cards[0].value)


    def __hash__(self):
        return hash(self.equivalence_class())


class Strategy(object):

    def clone(self):
        s = self.__class__()
        return s


    def observe_move(self, player, move):
        pass


    def observe_win(self, winner):
        pass


    def choose_move(self):
        raise NotImplementException


class KeyboardStrategy(Strategy):

    def choose_move(self):
        print 'Pile is:', self.game.pile, 'owned by',self.game.players[self.game.pile_owner].name
        print 'Your hand:', self.owner.hand
        moves = self.owner.valid_moves()
        choices = zip(range(len(moves)), moves)
        print 'Your moves:', ', '.join(['%d %s' % p for p in choices])
        while True:
            choice = input('?')
            if choice in range(len(moves)):
                break
        move = moves[choice]
        print 'Move chosen:', move
        return move


class RandomStrategy(Strategy):

    def choose_move(self):
        move_list = self.owner.valid_moves()
        m = len(move_list)
        n = randrange(0, m)
        move = move_list[n]
        return move


class LowestCardStrategy(Strategy):

    def choose_move(self):
        move_list = self.owner.valid_moves()
        move_list.sort()
        move = move_list[0]
        if move.is_empty() and len(move_list) > 1:
            move = move_list[1]
        return move


class LowestHalfStrategy(Strategy):

    def choose_move(self):
        move_list = self.owner.valid_moves()
        m = int(ceil((len(move_list)+1)/2.0))
        n = randrange(0, m)
        move = move_list[n]
        if move.is_empty() and len(move_list) > 1:
            move = move_list[1]
        return move


class LearningStrategy(Strategy):

    def __init__(self):
        self.wins = {}
        self.move_history = []


    def observe_win(self, winner):
        if winner == self.owner:
            for m in self.move_history:
                if m.is_empty():
                    continue
                ec = m.equivalence_class()
                self.wins[ec] = self.wins.get(ec, 0) + 1
        
        self.move_history = []
        l = [(b, a) for a,b in self.wins.iteritems()]
        l.sort()


    def choose_move(self):
        move_list = self.owner.valid_moves()
        
        def win_cmp(a, b):
            ac = a.equivalence_class()
            bc = b.equivalence_class()
            c = cmp(self.wins.get(bc, randint(0, 3)), self.wins.get(ac, randint(0, 3)))
            return c
        
        move_list.sort(win_cmp)
        print ' '.join('%s/%s' % (m, self.wins.get(m.equivalence_class(), 0)) for m in move_list)
        move = move_list[0]
        self.move_history.append(move)
        return move


class ScenarioStrategy(Strategy):
    """This strategy works by doing the following:
    
    1. Generate N scenarios, each with the hidden cards randomly dealt to the
    other players according to how many cards they are known to hold.
    
    2. For each scenario, for each move, evaluate the value of that move in
    that scenario (using a value function that is designed for perfect information,
    e.g. max_n).
    
    3. Average the values for each move, and choose the move that has the best
    average score."""
    
    def __init__(self, value_function, num_scenarios = 100):
        self.value_function = value_function
        self.num_scenarios = num_scenarios

    def clone(self):
        s = ScenarioStrategy(self.value_function, self.num_scenarios)
        return s

    def choose_move(self):
        best_move = None
        best_value = -1000000
        scenarios = list(self.generate_scenarios())
        moves = self.owner.valid_moves()
        #print 'moves',' '.join(str(m) for m in moves)
        if len(moves) == 0:
            print self.game
            raise '!'
        if len(moves) == 1:
            return moves[0]
        for m in moves:
            total = 0
            for game in scenarios:
                g = game.clone()
                g.do_move(m)
                total += self.value_function(g, game.current_player)
            #print 'Move', m,'total', total
            if total > best_value:
                best_value = total
                #print 'better move: ', m, 'old: ', best_move
                best_move = m
        
        #if best_move.is_empty() and len(moves) > 1:
        #    print 'Interesting: noncompulsory pass, others', ' '.join(str(m) for m in moves)
        
        #if not best_move.is_empty() and best_move.cards[0].value < 16 and len(best_move) < len(self.owner.hand.sets[best_move.cards[0].value]):
        #    print 'Interesting: splitting set, total', len(self.owner.hand.sets[best_move.cards[0].value])
        
        return best_move
    
    def generate_scenarios(self):
        for i in range(self.num_scenarios):
            g = self.game.clone()
            g.silent = True
            self.deal_hidden_cards(g)
            yield g
    
    def deal_hidden_cards(self, game):
        hidden_cards = Deck()
        for p in game.players:
            if p.name != self.owner.name:
                hidden_cards.add_all(p.hand)
                p.pre_scenario_hand = p.hand
                p.hand = Hand()
        cards2 = hidden_cards.shuffle()
        
        i = 0
        p = game.players[i]
        for c in cards2:
            while p.name == self.owner.name or len(p.hand) >= len(p.pre_scenario_hand):
                i += 1
                p = game.players[i]
            p.hand.add(c)
        
        for p in game.players:
            try:
                del p.pre_scenario_hand
            except AttributeError:
                pass


class Player(object):
    def __init__(self, game, name, strategy):
        self.game = game
        self.name = name
        self.hand = Hand()
        self.strategy = strategy
        strategy.owner = self
        strategy.game = game
        self.score = 0
        self.rank = -1


    def clone(self, game, clone_hand=True):
        p = Player(game, self.name, self.strategy.clone())
        if clone_hand:
            p.hand = self.hand.clone()
        else:
            p.hand = self.hand
        p.score = self.score
        p.rank = self.rank
        return p


    def play_cards(self, move):
        self.hand.remove_all(move)


    def valid_moves(self):
        move_list = [m for m in self.hand.moves() if self.game.is_valid_move(m)]
        return move_list


    def observe_move(self, player, move):
        self.strategy.observe_move(player, move)


    def observe_win(self, winner):
        self.strategy.observe_win(winner)


    def choose_move(self):
        move = self.strategy.choose_move()
        return move


    def __str__(self):
        return "%s %s" % (self.name, self.hand.__str__())


class Game(object):
    def __init__(self):
        self.players = []
        self.pile = Deck()
        self.discards = Deck()
        self.current_player = 0
        self.pile_owner = 0
        self.pass_count = 0
        self.players_out = 0
        self.silent = False
    
    def clone(self, for_player=None):
        g = Game()
        
        g.players = []
        for i in range(len(self.players)):
            g.players.append(self.players[i].clone(g, for_player is None or for_player == i))
            
        g.pile = self.pile.clone()
        g.discards = self.discards.clone()
        g.current_player = self.current_player
        g.pile_owner = self.pile_owner
        g.pass_count = self.pass_count
        g.players_out = self.players_out
        g.silent = self.silent
        return g
    
    def add_player(self, name, strategy):
        p = Player(self, name, strategy)
        self.players.append(p)
        return p
    
    def deal(self):
        num_packs = int(len(self.players)/4.0 + 0.999)
        pack = Deck()
        pack.full_pack(num_packs)
        pack2 = pack.shuffle()
        
        for p in self.players:
            p.hand.empty()
        
        i = randrange(0, len(self.players))
        for c in pack2:
            self.players[i % len(self.players)].hand.add(c)
            i = i+1
    
    def __str__(self):
        return "%s (%d), %d [%s]" % (self.pile, self.pile_owner, self.current_player, '\n'.join([p.__str__() for p in self.players]))

    def is_valid_move(self, move):
        if self.pile_owner == self.current_player:
            if self.players[self.current_player].hand.is_empty():
                return True
            return not move.is_empty()
        if self.pile.is_empty() and move.is_empty():
            return False
        if self.pile.is_empty():
            return True
        if move.is_empty():
            return True
        if self.pile.first_card().value >= 16:
            return False
        if move.first_card().value >= 16:
            return True
        if not move.first_card().outranks(self.pile.first_card()):
            return False
        if len(move.cards) != len(self.pile.cards):
            return False
        return True


    def is_won(self):
        return self.players_out+1 >= len(self.players)


    def do_move(self, move):
        player = self.players[self.current_player]
        if not player.hand.is_empty():
            if move.is_empty():
                self.pass_count += 1
            else:
                self.pass_count = 0
        if self.pass_count >= len(self.players):
            print self
            print move
            print self.is_valid_move(move)
            raise '!'
        player.play_cards(move)
        if not move.is_empty():
            self.discards.add_all(self.pile)
            self.pile = move
            self.pile_owner = self.current_player
        elif player.hand.is_empty() and not self.pile.is_empty():
            self.pile = move
            self.pass_count = 0
        if not move.contains(Card(JOKER_VALUE, 'J')):
            self.current_player = (self.current_player+1) % len(self.players)
        
        self.observe_move(player, move)
        if not move.is_empty() and player.hand.is_empty():
            player.score = (1 - self.players_out)*1000
            player.rank = self.players_out
            self.players_out += 1
            self.observe_score(player)
            if self.is_won():
                for p in self.players:
                    if not p.hand.is_empty():
                        p.score = (1 - self.players_out)*1000
                        p.rank = self.players_out
                        self.observe_score(p)


    def observe_move(self, player, move):
        if not self.silent:
            print "Move:", player.name, move,len(player.hand), 'remaining'
        for p in self.players:
            p.observe_move(player, move)
    
    
    def observe_score(self, winner):
        if not self.silent:
            print "Score:", winner.name, winner.score
        #for p in self.players:
        #    p.observe_score(winner)


    def observe_win(self, winner):
        if not self.silent:
            print "Winner:", winner.name
        for p in self.players:
            p.observe_win(winner)


def make_max_n(depth):
    def max_n(game, player):
        """What is the value of this game state?"""
        
        used_depth = depth + int((len(game.discards)/10.0)**1.5)
        global tree_size
        tree_size = 0
        #start_time = clock()
        vector = max_n_vector(game, game.current_player, used_depth)
        #end_time = clock()
        #print tree_size, end_time-start_time, tree_size/(end_time-start_time)
        return vector[player]
    
    return max_n


def max_n_vector(game, player, depth = 1):
    global tree_size
    tree_size += 1
    
    if game.is_won():
        vector = [0]*len(game.players)
        for i in range(len(game.players)):
            vector[i] = game.players[i].score
    elif depth <= 0:
        vector = [0]*len(game.players)
        for i in range(len(game.players)):
            vector[i] = 2*game.players[i].hand.total_value()
            for j in range(len(game.players)):
                if i != j:
                    vector[i] -= game.players[j].hand.total_value()
            s = sum(vector)/3
            for i in range(len(game.players)):
                vector[i] -= s
    else:
        player = game.current_player
        moves = game.players[game.current_player].valid_moves()
        best_move = None
        vector = [-1000000]*len(game.players)
        for m in moves:
            g = game.clone(player)
            g.do_move(m)
            v = max_n_vector(g, game.current_player, depth-1)
            if v[player] > vector[player]:
                best_move = m
                vector = v
    
    #print "V", depth, game.current_player, vector
    return vector


value_map = {
    3: 0,
    4: 1,
    5: 2,
    6: 3,
    7: 4,
    8: 5,
    9: 6,
    10: 7,
    11: 8,
    12: 9,
    13: 10,
    14: 11,
    15: 12,
    JOKER_VALUE: 15
}

def make_rankoids_ai(depth):
    def rankoids_ai(game, player):
        import rankoids_ai as r
        
        r.set('depth', depth)
        r.set('current_player_bonus', 1)
        r.set('hand_size_bonus', -20)
        
        pile_value = game.pile.cards[0].value
        pile_count = len(game.pile)
        r.init_game(len(game.players), game.current_player, game.pile_owner, (pile_value, pile_count))
        ranked = []
        for i in range(len(game.players)):
            if len(game.players[i].hand.cards) == 0:
                ranked.append((game.players[i].rank, i))
        ranked.sort()
        for rank,i in ranked:
            r.rank_player(i)
            
        for i in range(len(game.players)):
            for c in game.players[i].hand.cards:
                value = value_map[c.value]
                r.add_card(i, value)
        
        vector = r.evaluate_game()
        
        return vector[player]
    
    return rankoids_ai


my_learning_strategy = LearningStrategy()


def simulate():
    game = Game()
    p1 = game.add_player('Mugwump', ScenarioStrategy(make_max_n(3), 1))
    p2 = game.add_player('Fritha', ScenarioStrategy(make_max_n(3), 1))
    p3 = game.add_player('Buggles', ScenarioStrategy(make_rankoids_ai(9), 1))
    #p4 = game.add_player('Edmund', KeyboardStrategy())
    #game.silent = True
    shuffle(game.players)
    game.deal()
    print game
    for i in range(100):
        player = game.players[game.current_player]
        move = player.choose_move()
        game.do_move(move)
        if game.is_won():
            break
    print game
    #for p in game.players:
    #    print p.name,p.score
    
    table = sorted([(p.score, p.name) for p in game.players])
    return tuple([n for s,n in reversed(table)])


def run():
    results = {}
    num_simulations = 30
    start_time = clock()
    for i in range(num_simulations):
        scores = simulate()
        print i, scores
        results[scores] = results.setdefault(scores, 0) + 1
    end_time = clock()
    print end_time-start_time, num_simulations/(end_time-start_time)
    print '\n'.join(['%s: %d' % (s, f) for s, f in results.iteritems()])


def test1():
    game = Game()
    p1 = game.add_player('Mugwump', ScenarioStrategy(make_max_n(5), 1))
    p2 = game.add_player('Edmund', KeyboardStrategy())
    p3 = game.add_player('Fritha', ScenarioStrategy(make_max_n(5), 1))
    p1.hand = Hand()
    p2.hand = Hand([Card(6, 'S')])
    p3.hand = Hand([Card(6, 'D'), Card(8, 'D'), Card(11, 'S')])
    game.pile = Deck([Card(10, 'S')])
    game.pile_owner = 0
    game.current_player = 2
    print game
    print ', '.join(str(m) for m in p3.valid_moves())
    print p3.choose_move()


try:
    import psyco
    psyco.full()
    print 'Optimised'
except ImportError:
    print 'Not optimised'

run()
#test1()
