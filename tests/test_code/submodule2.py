import test_code.submodule1 as b

from . import submodule1

A = 32


def test_2(a):
    return submodule1.test_func2(a) + A + b.test_func1(a)
