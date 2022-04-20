import numpy as np
from numba import njit

@njit
def get_arr(i,j):
    a = np.arange(i,j)
    return a[np.bitwise_and(a%5 != 0, a%3 != 0)]

def fizzbuzzv3(chunk,length):
    string = ""
    for i in range(1,chunk+1):
        if i%15 == 0:
            string += "FizzBuzz"
        elif i%3 == 0:
            string += "Fizz"
        elif i%5 == 0:
            string += "Buzz"
        else:
            string += "{}"
        string += "\n"
    string = string[:-2]
    for i in range(1,length,chunk):
        print(string.format(*get_arr(i,i+chunk)))
        
fizzbuzzv3(6000,int(1e100))
