import sys
import ontology
from vocabulary import nouns, relative_nouns, verbs, adjectives, conjunctions, articles, prepositions, known_words


rules = {}

def add_rule(head, body):
    if head not in rules:
        rules[head] = []
    rules[head].append(body)


class ParseError(Exception): pass

class Nonterminal(object):
    TERMINAL = False
    
    def __init__(self, parts):
        self.parts = parts
        self.score = 1.0
        for p in parts:
            self.score *= p.score
    
    def __str__(self):
        return '%s:(%s)' % (type(self).__name__, ' '.join([str(p) for p in self.parts]))
    
    def get_terminals(self):
        for p in self.parts:
            for t in p.get_terminals():
                yield t
    
    def get_statements(self):
        if isinstance(self, Statement):
            yield self
        for p in self.parts:
            for s in p.get_statements():
                yield s

    def get_adjectives(self):
        if isinstance(self, Adjective):
            yield self
        for p in self.parts:
            for a in p.get_adjectives():
                yield a
            
class Terminal(object):
    TERMINAL = True
    
    def __init__(self, word):
        if type(word) is not str:
            raise Exception('Unexpected input to Terminal: %s' % word)
        
        self.score = 1.0
        if not self.parse(word):
            self.score *= self.guess_score(word)
            if word in known_words:
                self.score *= 0.5
        
        self.word = word
    
    def guess_score(self, word):
        return 0.8
    
    def __str__(self):
        if self.score < 1.0:
            extra = '(%d)' % int(self.score*100)
        else:
            extra = ''
        return '%s%s:%s' % (type(self).__name__, extra, self.word)
    
    def get_terminals(self):
        yield self
    
    def get_statements(self):
        return []

    def get_adjectives(self):
        return []
        
class Empty(object): pass

class Sentence(Nonterminal): pass
class Statement(Nonterminal): pass
class NounPhrase(Nonterminal):
    def get_head(self):
        for p in self.parts:
            if isinstance(p, Noun):
                return p.get_head()
            if isinstance(p, RelativeNounWord):
                return p
        raise ParseError('No head in NounPhrase?')
class VerbPhrase(Nonterminal):
    def get_verb(self):
        return self.parts[0].get_verb()
class IntransitiveVerbPhrase(Nonterminal):
    def get_verb(self):
        return self.parts[0].get_verb()
class Noun(Nonterminal):
    def get_head(self):
        for p in self.parts:
            if isinstance(p, NounWord) or isinstance(p, RelativeNounWord):
                return p
        raise ParseError('No head in Noun?')
class Verb(Nonterminal):
    def get_verb(self):
        return self.parts[0]
class Adjectives(Nonterminal): pass
class Adjective(Nonterminal): pass
class Conjunction(Nonterminal): pass
class Article(Nonterminal): pass
class AdjectiveClause(Nonterminal): pass
class PrepositionPhrase(Nonterminal): pass
class Preposition(Nonterminal): pass
class Command(Nonterminal): pass
    

class NounWord(Terminal):
    def parse(self, word):
        return word in nouns
    
    def get_annotation(self):
        return 'n'
    
    def guess_score(self, word):
        return 0.9
    
    def register(self):
        nouns.append(self.word)

class RelativeNounWord(Terminal):
    def parse(self, word):
        return word in relative_nouns
    
    def get_annotation(self):
        return 'rn'

    def register(self):
        relative_nouns.append(self.word)

class VerbWord(Terminal):
    def parse(self, word):
        return word in verbs
    
    def get_annotation(self):
        return 'v'
    
    def guess_score(self, word):
        if word[-1] == 'd':
            return 0.9
        return 0.8

    def register(self):
        verbs.append(self.word)

class AdjectiveWord(Terminal):
    def parse(self, word):
        return word in adjectives
    
    def get_annotation(self):
        return 'adj'
    
    def register(self):
        adjectives.append(self.word)

class ConjunctionWord(Terminal):
    def parse(self, word):
        return word in conjunctions
    
    def get_annotation(self):
        return 'conj'
    
    def register(self):
        conjunctions.append(self.word)

class ArticleWord(Terminal):
    def parse(self, word):
        return word in articles
    
    def get_annotation(self):
        return 'a'
    
    def register(self):
        articles.append(self.word)

class PrepositionWord(Terminal):
    def parse(self, word):
        return word in prepositions
    
    def get_annotation(self):
        return 'p'
    
    def register(self):
        prepositions.append(self.word)

add_rule(Sentence, [Statement])
add_rule(Sentence, [Statement, Conjunction, Sentence])
add_rule(Statement, [NounPhrase, VerbPhrase])
add_rule(NounPhrase, [Noun])
add_rule(NounPhrase, [RelativeNounWord])
add_rule(NounPhrase, [Noun, AdjectiveClause])
add_rule(NounPhrase, [Article, Noun])
add_rule(NounPhrase, [Article, Adjectives, Noun])
add_rule(NounPhrase, [Article, Noun, AdjectiveClause])
add_rule(NounPhrase, [Article, Adjectives, Noun, AdjectiveClause])
add_rule(NounPhrase, [Adjectives, Noun])
add_rule(NounPhrase, [Adjectives, Noun, AdjectiveClause])
add_rule(NounPhrase, [Article, Adjectives, Noun])
add_rule(VerbPhrase, [Verb])
add_rule(VerbPhrase, [Verb, NounPhrase])
add_rule(VerbPhrase, [Verb, PrepositionPhrase])
add_rule(IntransitiveVerbPhrase, [Verb])
add_rule(IntransitiveVerbPhrase, [Verb, PrepositionPhrase])
add_rule(Noun, [NounWord])
add_rule(Verb, [VerbWord])
add_rule(Adjective, [AdjectiveWord])
add_rule(Adjectives, [Adjective])
add_rule(Adjectives, [Adjective, Conjunction, Adjectives])
add_rule(Conjunction, [ConjunctionWord])
add_rule(Article, [ArticleWord])
add_rule(AdjectiveClause, [RelativeNounWord, NounPhrase, IntransitiveVerbPhrase])
add_rule(AdjectiveClause, [RelativeNounWord, VerbPhrase])
add_rule(PrepositionPhrase, [Preposition, NounPhrase])
add_rule(Preposition, [PrepositionWord])

add_rule(Sentence, [Command])
add_rule(Command, [VerbPhrase])


SCORE_THRESHOLD = 0.33


def parse_body(words, body, score=1.0):
    if len(body) == 0:
        yield [], words
        return
    for first_part, remainder in parse(words, body[0], score):
        new_score = score*first_part.score
        if new_score < SCORE_THRESHOLD:
            continue
        for parts, remainder2 in parse_body(remainder, body[1:], new_score):
            yield [first_part] + parts, remainder2

def parse(words, type, score=1.0):
    """Parse some of the words into an object of the type.  Yields all possible result objects and remaining words."""
    
    if len(words) == 0:
        #yield Empty(), []
        return
    
    if type.TERMINAL:
        try:
            result = type(words[0])
        except ParseError:
            return
        yield result, words[1:]
        return
    
    if type in rules:
        bodies = rules[type]
    else:
        bodies = []
    for body in bodies:
        for parts, remainder in parse_body(words, body, score):
            try:
                result = type(parts)
            except ParseError:
                continue
            yield result, remainder


def annotation(result):
    if result.TERMINAL:
        ann = result.get_annotation()
        if result.score < 1.0:
            cert = '?'
        else:
            cert = ':'
        s = '%s%s%s' % (ann, cert, result.word)
    else:
        s = ' '.join([annotation(p) for p in result.parts])
    return s


o = ontology.Ontology()


def embellish_query(tree, query):
    if isinstance(tree, VerbPhrase):
        verb_word = tree.get_verb()
        #verb = ontology.Action(verb_word.word)
        verb = o.lookup_action(verb_word.word)
        obj = None
        if len(tree.parts) >= 2 and isinstance(tree.parts[1], NounPhrase):
            obj = build_query(tree.parts[1])
        query.criteria.append((verb, obj))
    elif isinstance(tree, Adjectives):
        for a in tree.get_adjectives():
            #verb = ontology.Action('is')
            verb = o.lookup_action('is')
            obj_word = a.parts[0]
            obj = o.lookup_thing(obj_word.word)
            query.criteria.append((verb, obj))
    elif isinstance(tree, AdjectiveClause):
        if isinstance(tree.parts[1], NounPhrase):
            #add_rule(AdjectiveClause, [RelativeNounWord, NounPhrase, IntransitiveVerbPhrase])
            obj = build_query(tree.parts[1])
            verb_word = tree.parts[2].get_verb()
            #verb = ontology.Action(verb_word.word)
            verb = o.lookup_action(verb_word.word)
            query.criteria.append((obj, verb))
        elif isinstance(tree.parts[1], VerbPhrase):
            #add_rule(AdjectiveClause, [RelativeNounWord, VerbPhrase])
            embellish_query(tree.parts[1], query)
        else:
            print >>sys.stderr, "Unhandled AdjectiveClause!"
    elif isinstance(tree, Noun):
        pass
    elif isinstance(tree, Article):
        pass
    else:
        print >>sys.stderr, "Can't embellish query from %s" % type(tree)
    return query


def build_query(tree):
    if isinstance(tree, Statement):
        query = build_query(tree.parts[0])
        query = embellish_query(tree.parts[1], query)
    elif isinstance(tree, NounPhrase):
        noun = tree.get_head()
        subject = o.lookup_thing(noun.word)
        query = ontology.Query(subject)
        for p in tree.parts:
            embellish_query(p, query)
    else:
        print >>sys.stderr, "Can't build query from %s" % type(tree)
        return None
    return query


def comprehend_tree(ontology, tree):
    for s in tree.get_statements():
        q = build_query(s)
        best = None
        for r in q.run(ontology):
            if best is None or r.get_score(ontology) < best.get_score(ontology):
                best = r
            print r.get_score(ontology), r.describe()
        best.apply(ontology)
    
    f = open('ontology.dot', 'wt')
    f.write(ontology.to_dot())
    f.close()


def parse_sentence(sentence, register=False):
    words = sentence.split(' ')
    scores = []
    for result, remainder in parse(words, Sentence):
        if len(remainder) == 0:
            scores.append((-result.score, result))
    scores.sort()
    best = scores[0][1]
    new_words = []
    for t in best.get_terminals():
        if t.score < 1.0:
            if register:
                t.register()
            new_words.append(t)
    comprehend_tree(o, best)
    return new_words


def test(sentence=None):
    if sentence is None:
        sentence = 'this is the maid that stroked the cat that caught the rat that ate the grain that fed the farmer that lived in the house that jack built'
    words = sentence.split(' ')
    scores = []
    for result, remainder in parse(words, Sentence):
        if len(remainder) == 0:
            scores.append((-result.score, result))
    scores.sort()
    for score, result in scores[:5]:
        print -score, annotation(result), result
        print


if __name__ == '__main__':
    try:
        import psyco
        psyco.full()
    except ImportError:
        pass
    
    test()
    