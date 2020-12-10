# -*- coding: utf-8; -*-
# See issue #2

"""
This works fine
a = 3
b = 4
print(a + b)
"""

# But this did not (#2)
a: int = 3
b = 4
print(a + b)
