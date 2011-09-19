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
    
class Empty(object): pass

class Sentence(Nonterminal): pass
class Statement(Nonterminal): pass
class NounPhrase(Nonterminal): pass
class VerbPhrase(Nonterminal): pass
class IntransitiveVerbPhrase(Nonterminal): pass
class Noun(Nonterminal): pass
class Verb(Nonterminal): pass
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

class RelativeNounWord(NounWord):
    def parse(self, word):
        return word in relative_nouns
    
    def get_annotation(self):
        return 'rn'

    def register(self):
        nouns.append(self.word)
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

nouns = ['i', 'jellybeans', 'jack', 'house', 'this', 'that', 'cat', 'rat', 'maid', 'farmer', 'grain']
relative_nouns = ['that']
verbs = ['is', 'am', 'likes', 'be', 'built', 'ate', 'caught', 'stroked']
adjectives = ['green', 'red']
conjunctions = ['and']
articles = ['the', 'a', 'an']
prepositions = []
known_words = nouns + verbs + adjectives + conjunctions + articles + prepositions

rules = {}

def add_rule(head, body):
    if head not in rules:
        rules[head] = []
    rules[head].append(body)

add_rule(Sentence, [Statement])
add_rule(Sentence, [Statement, Conjunction, Sentence])
add_rule(Statement, [NounPhrase, VerbPhrase])
add_rule(NounPhrase, [Noun])
add_rule(NounPhrase, [Noun, AdjectiveClause])
add_rule(NounPhrase, [Article, Noun])
add_rule(NounPhrase, [Article, Adjectives, Noun])
add_rule(NounPhrase, [Article, Noun, AdjectiveClause])
add_rule(NounPhrase, [Article, Adjectives, Noun, AdjectiveClause])
add_rule(NounPhrase, [Adjectives])
add_rule(NounPhrase, [Adjectives, Noun, AdjectiveClause])
add_rule(NounPhrase, [Article, Adjectives, NounPhrase])
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
    return new_words


def test(sentence=None):
    if sentence is None:
        sentence = 'this is the maid that stroked the cat that caught the rat that ate the grain that fed the farmer that lived in the house that jack built'
    words = sentence.split(' ')
    print len(words)
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
    