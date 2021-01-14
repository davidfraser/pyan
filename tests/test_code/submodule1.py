from test_code import subpackage1 as subpackage
from test_code.subpackage1 import A


def test_func1(a):
    return a


def test_func2(a):
    return a


class B:
    def __init__(self, k):
        self.a = 1

    def to_A(self):
        return A(self)

    def get_a_via_A(self):
        return test_func1(self.to_A().b.a)
