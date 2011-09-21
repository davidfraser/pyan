nouns = []
relative_nouns = []
verbs = []
adjectives = []
conjunctions = []
articles = []
prepositions = []
known_words = []


def define_relative_noun(word):
    relative_nouns.append(word)

def define_noun(word):
    nouns.append(word)

def define_verb(word):
    verbs.append(word)

def define_conjunction(word):
    conjunctions.append(word)

def define_article(word):
    articles.append(word)

def define_preposition(word):
    prepositions.append(word)

define_relative_noun('this')
define_relative_noun('that')
define_noun('i')
define_noun('you')
define_noun('me')
define_verb('is')
define_verb('am')
define_conjunction('and')
define_conjunction('but')
define_conjunction('unless')
define_conjunction('therefore')
define_article('the')
define_article('a')
define_article('an')
define_preposition('in')
define_preposition('by')
define_preposition('for')

known_words = nouns + verbs + adjectives + conjunctions + articles + prepositions
