import numpy as np
import os
def fizzbuzz(chunk,length):
    string_arr = np.empty(chunk).astype('<U8')
    string_arr[:] = '%d'
    string_arr[::3] = 'Fizz'
    string_arr[::5] = 'Buzz'
    string_arr[::15] = 'FizzBuzz'
    string = '\n'.join(string_arr) + '\n'
    offset_arr = np.arange(chunk)
    offset_arr = (offset_arr%5 != 0)&(offset_arr%3 != 0)
    offset_arr = np.where(offset_arr)[0]
    for i in range(0,length,chunk):
        to_output = string%tuple(offset_arr.tolist())
        os.write(1,to_output.encode())
        offset_arr += chunk
fizzbuzz(8100,int(1e100))
