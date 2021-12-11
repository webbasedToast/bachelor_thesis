import numpy as np
from random import gauss, seed
from pandas import Series


"""
This module is used to generate a series of gaussian random values which are distributed between 0 and 1.
The numbers in this series are then normalized to all be 0 or positive numbers and afterwards
they get scaled upwards by factor 1000 and cast to integers.

The series of numbers is used as input for a pseudo-white-noise sound sample.
"""
seed(1)
series = [gauss(0.0, 1.0) for i in range(1200)]
series = Series(series)

cache = np.asarray(series)
cache[cache < 0.] = 0.0

for i, j in enumerate(cache):
    if j < 1000:
        cache[i] *= 1000
        cache[i] = int(cache[i])

cache = cache.tolist()
cache = [int(i) for i in cache]
print(cache)
