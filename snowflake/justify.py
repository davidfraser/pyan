import sys
import math
from numpy import std

def split(points, pos):
    i = 0
    l = 0
    while i < len(points):
        l += points[i]
        if l == pos:
            return [i]
        if l > pos:
            if i < 1:
                return [i]
            else:
                return [i-1,i]
        i += 1
    return [i-1]

def score(points, line_len):
    return sum(map(lambda x: abs(x-line_len)**2.5, points))
    #dists = points
    #s = sum(dists)**2 - sum(map(lambda x: x**2, dists))
    #print >>sys.stderr, "score(%s) = %s" % (points, s)
    #return s
    #return std(points)

try_justify_cache = {}

try_justify_calls = 0
try_justify_cache_hits = 0

def try_justify(points, line_len):
    global try_justify_calls, try_justify_cache_hits
    try_justify_calls += 1
    points = tuple(points)
    if (points, line_len) in try_justify_cache:
        try_justify_cache_hits += 1
        return try_justify_cache[(points, line_len)]
    if len(points) <= 1:
        return [list(points)]
    if len(points) <= 2 or sum(points) <= line_len:
        return [[sum(points)]]
    js = []
    #print 'split', points, line_len
    split_pos = split(points, line_len)
    #print 'split_pos', split_pos
    for p in split_pos:
        offset = sum(points[:p+1])
        remaining_points = points[p+1:]
        #print 'try_justify',remaining_points,line_len
        for sub_j in try_justify(remaining_points, max(line_len, offset)):
            #print 'result',sub_j
            j = [offset] + sub_j
            js.append(j)
    try_justify_cache[(points, line_len)] = js
    return js

def justify_text(points, ratio=1.0):
    if len(points) == 1:
        return [(0,points)]
    if len(points) == 2:
        return [(0,points)]
    
    num_lines = math.sqrt(sum(points)/ratio)
    line_len = int(math.ceil(sum(points) / num_lines))
    maxp = max(points)
    if maxp > line_len:
        line_len = maxp
    all_js = []
    for j in set([tuple(j) for j in try_justify(points, line_len)]):
        s = score(j, line_len)
        all_js.append((s, j))
    all_js.sort()
    return all_js

def render_text(t, j):
    st = []
    l = 0
    l0 = 0
    for p in j:
        l += p
        st.append(t[l0:l].strip())
        l0 = l
    return '\n'.join(st)

def get_points(t):
    p = []
    for w in t.split():
        p.append(len(w) + 1)
    if len(p) > 0:
        p[-1] = p[-1] - 1
    return p


if __name__ == '__main__':
    import psyco
    psyco.full()

    #t = "the quick brown fox jumps over the lazy dog"
    #t = "once upon a time there was a little man called tim"
    #t = "three rings for elven kings under the sky. seven for the dwarf-lords in their halls of stone. nine for mortal men doomed to die. one for the dark lord on his dark throne. in the land of morder where the shadows lie."
    #t = t + " one ring to rule them all. one ring to find them. one ring to bring them all and in the darkness bind them. in the land of morder where the shadows lie."
    #t = t + " " + t
    t = "NosFxoMUREX and PremFXOMurex, appear to just load subset of FXOs"
    #t = "C++ component: trigger information is loaded into strikePrice, Put/Call code"
    points = get_points(t)
    all_js = justify_text(points, 2.5)
    for s,j in all_js:
        print render_text(t, j)
        print s
        print
    print try_justify_calls, try_justify_cache_hits, len(all_js)
