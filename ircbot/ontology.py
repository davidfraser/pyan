import sys


class Thing(object):
    """A thing vertex in the ontology."""
    def __init__(self, name):
        self.name = name
    
    def __str__(self):
        return """<Thing '%s' at 0x%x>""" % (self.name, id(self))


class Action(object):
    """An action vertex in the ontology."""
    def __init__(self, name):
        self.name = name

    def __str__(self):
        return """<Action '%s' at 0x%x>""" % (self.name, id(self))


class Ontology(object):
    """An ontology of Things and Actions.  Each of these is defined by its name, but many instances of a thing or action may occur (and each is a separate object).  Each instance is a vertex;
    an "edge" is a tuple (subj, verb, obj) or (subj, verb, None)."""
    
    def __init__(self, base=None):
        self.base = base
        self.things = {}
        self.actions = {}
        self.subject_edges = {}
        self.object_edges = {}
        self.verb_edges = {}
    
    def add_thing(self, thing):
        name = thing.name
        if name not in self.things:
            self.things[name] = []
        self.things[name].append(thing)
        self.subject_edges[thing] = []
        self.object_edges[thing] = []

    def add_action(self, action):
        name = action.name
        if name not in self.actions:
            self.actions[name] = []
        self.actions[name].append(action)
        self.verb_edges[action] = []

    def add_edge(self, subj, verb, obj=None):
        self.subject_edges[subj].append((verb, obj))
        self.verb_edges[verb].append((subj, obj))
        if obj is not None:
            self.object_edges[obj].append((subj, verb))
    
    def get_things(self, name):
        if name not in self.things:
            return []
        return self.things[name]

    def get_actions(self, name):
        if name not in self.actions:
            return []
        return self.actions[name]

    def get_subject_edges(self, subj):
        if subj not in self.subject_edges:
            return []
        return self.subject_edges[subj]

    def get_object_edges(self, obj):
        if obj not in self.object_edges:
            return []
        return self.object_edges[obj]

    def get_verb_edges(self, verb):
        if verb not in self.verb_edges:
            return []
        return self.verb_edges[verb]
    
    def has_edge(self, subj, verb, obj=None):
        for v, o in self.get_subject_edges(subj):
            #print 'comparing (%s, %s, %s) and (%s, %s, %s)' % (subj, v, o, subj, verb, obj)
            if v == verb and o == obj:
                return True
        return False
    
    def to_dot(self):
        s = """digraph G {\n"""
        for name in self.things:
            for subj in self.things[name]:
                s += """    thing_%x [label="%s"];\n""" % (id(subj), subj.name)
        for name in self.things:
            for subj in self.things[name]:
                for verb, obj in self.get_subject_edges(subj):
                    if obj is None:
                        s += """    nowhere_%x%x [label="", shape=none]\n""" % (id(subj), id(verb))
                        s += """ thing_%x -> nowhere_%x%x [label="%s"];\n""" % (id(subj), id(subj), id(verb), verb.name)
                    else:
                        s += """    thing_%x -> thing_%x [label="%s"];\n""" % (id(subj), id(obj), verb.name)
        s += """}\n"""
        return s
    
    def lookup_action(self, name):
        if name not in self.actions or len(self.actions[name]) == 0:
            self.add_action(Action(name))
        return self.actions[name][0]
    
    def lookup_thing(self, name):
        if name not in self.things or len(self.things[name]) == 0:
            self.add_thing(Thing(name))
        return self.things[name][0]


class Query(object):
    """A Query can be used to find a vertex based on the relationships it has with other vertices.
        When run it yields a set of Result objects that represent ways of applying the query to the ontology."""
    def __init__(self, target, criteria=None):
        self.target = target
        if criteria is None:
            criteria = []
        self.criteria = criteria
    
    def run(self, ontology):
        if self.target is None:
            yield Result(ontology, None, [])
            return
        
        subresults = []
        for c in self.criteria:
            a, b = c
            if isinstance(a, Action):
                q = b
            else:
                q = a
            if isinstance(q, Thing) or q is None:
                q = Query(q)
            results = []
            for r in q.run(ontology):
                results.append(r)
            subresults.append(results)
            
        for t in ontology.get_things(self.target.name):
            filtered_subresults = []
            for c, results in zip(self.criteria, subresults):
                filtered_results = []
                for r in results:
                    if self.criteria_match(t, c, r, ontology):
                        filtered_results.append(r)
                filtered_subresults.append(filtered_results)
        
            for r in self.compose_results(ontology, t, self.criteria, filtered_subresults):
                yield r
        
        for r in self.compose_results(ontology, self.target, self.criteria, subresults):
            yield r
    
    def criteria_match(self, t, c, r, ontology):
        a, b = c
        if isinstance(a, Action):
            subj = t
            verb = a
            obj = r.thing
        else:
            subj = r.thing
            verb = b
            obj = t
        a = ontology.get_actions(verb.name)
        if len(a) == 0:
            ontology.add_action(verb)
        else:
            verb = a[0]
        if not ontology.has_edge(subj, verb, obj):
            return False
        return True
    
    def compose_results(self, ontology, target, criteria, subresults):
        if len(subresults) == 0:
            yield Result(ontology, target, [])
        else:
            a,b = criteria[0]
            for r2 in self.compose_results(ontology, target, criteria[1:], subresults[1:]):
                for r in subresults[0]:
                    if isinstance(a, Action):
                        rel = a, r
                    else:
                        rel = r, b
                    yield Result(ontology, target, [rel], r2.rels)
    
    def __str__(self):
        s = '<Query %s [%s]>' % (self.target, ' '.join(['(%s %s)' % c for c in self.criteria]))
        return s


class Result(object):
    def __init__(self, ontology, thing, rels=None, extend_rels=None):
        self.ontology = ontology
        self.thing = thing
        if rels is None:
            rels = []
        self.rels = rels
        if extend_rels is not None:
            self.rels.extend(extend_rels)
    
    def describe(self, indent=0):
        if self.thing is None:
            return ''
        
        s = ''
        if self.thing not in self.ontology.get_things(self.thing.name):
            s += '    '*indent + 'Add vertex %s\n' % self.thing
        else:
            s += '    '*indent + 'Reuse vertex %s\n' % self.thing
        for a, b in self.rels:
            if isinstance(a, Action):
                subj = self.thing
                verb = a
                obj = b.thing
                subresult = b
            else:
                subj = a.thing
                verb = b
                obj = self.thing
                subresult = a
            s += subresult.describe(indent+1)
            if not self.ontology.has_edge(subj, verb, obj):
                s += '    '*indent + '    Add edge from %s through %s to %s\n' % (subj, verb, obj)
            else:
                s += '    '*indent + '    Reuse edge from %s through %s to %s\n' % (subj, verb, obj)
        return s
    
    def apply(self, ontology):
        """Apply this result to another ontology."""
        if self.thing is None:
            return
            
        if self.thing not in ontology.get_things(self.thing.name):
            ontology.add_thing(self.thing)
        for a, b in self.rels:
            if isinstance(a, Action):
                subj = self.thing
                verb = a
                obj = b.thing
                subresult = b
            else:
                subj = a.thing
                verb = b
                obj = self.thing
                subresult = a
            subresult.apply(ontology)
            if not ontology.has_edge(subj, verb, obj):
                if verb not in ontology.get_actions(verb.name):
                    ontology.add_action(verb)
                ontology.add_edge(subj, verb, obj)

    def get_score(self, ontology):
        """Score a result against an ontology by how much change would be applied."""
        if self.thing is None:
            return 0
        
        score = 0
        if self.thing not in ontology.get_things(self.thing.name):
            score += 10
        for a, b in self.rels:
            if isinstance(a, Action):
                subj = self.thing
                verb = a
                obj = b.thing
                subresult = b
            else:
                subj = a.thing
                verb = b
                obj = self.thing
                subresult = a
            score += subresult.get_score(ontology)
            if not ontology.has_edge(subj, verb, obj):
                if verb not in ontology.get_actions(verb.name):
                    score += 1
                score += 1
        return score

    def __str__(self):
        s = '<Result %s [%s]>' % (self.thing, ' '.join(['(%s %s)' % c for c in self.rels]))
        return s


def test():
    o = Ontology()
    q = Query(Thing('edmund'), [
        (Action('likes'), Query(Thing('jellybeans'), [
            (Action('are'), Query(Thing('green'))),
            (Query(Thing('people')), Action('own')),
        ])),
        (Query(Thing('people')), Action('includes')),
    ])
    for r in q.run(o):
        print >>sys.stderr, r.describe()
    
    r.apply(o)
    
    print o.to_dot()

if __name__ == '__main__':
    test()
