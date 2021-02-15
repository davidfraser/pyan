from ..submodule2 import test_2


class A:
    def __init__(self, b):
        self.b = test_2(b)
