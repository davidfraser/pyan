# -*- coding: utf-8; -*-
# See issue #3


def f():
    return [x for x in range(10)]


def g():
    return [(x, y) for x in range(10) for y in range(10)]


def h(results):
    return [
        (
            [(name, allargs) for name, _, _, allargs, _ in recs],
            {name: inargs for name, inargs, _, _, _ in recs},
            {name: meta for name, _, _, _, meta in recs},
        )
        for recs in (results[key] for key in sorted(results.keys()))
    ]
