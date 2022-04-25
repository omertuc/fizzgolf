import numpy as np
from os import write
from multiprocessing import shared_memory
from multiprocessing import Pool

def return_string(stringmemory_name,arraymemory_name,offset_arr_shape,offset):
    #recover string from shared bytes
    stringmemory = shared_memory.SharedMemory(name=stringmemory_name)
    fb_string = bytes(stringmemory.buf).decode()
    stringmemory.close()

    #recover numpy array from shared bytes
    arraymemory = shared_memory.SharedMemory(name=arraymemory_name)
    offset_arr = np.ndarray(offset_arr_shape, dtype=np.int64, buffer=arraymemory.buf) + offset
    arraymemory.close()

    #Get output
    to_output = fb_string % tuple(offset_arr.tolist())
    
    #Return encoded
    return to_output.encode()

def fizzbuzz(chunk,length,number_processes):

    #Make the string. Uses numpy arrays because it's easy
    string_arr = np.empty(chunk).astype('<U8')
    string_arr[:] = '%d'
    string_arr[::3] = 'Fizz'
    string_arr[::5] = 'Buzz'
    string_arr[::15] = 'FizzBuzz'
    fb_string = '\n'.join(string_arr.tolist()) + '\n'

    #Convert string to bytes and put it into shared memory
    fb_string = fb_string.encode()
    stringmemory = shared_memory.SharedMemory(create=True, size=len(fb_string))
    stringmemory.buf[:len(fb_string)] = fb_string

    #Make the offset array
    offset_arr = np.arange(chunk)
    offset_arr = (offset_arr%5 != 0)&(offset_arr%3 != 0)
    offset_arr = np.where(offset_arr)[0]

    #Put array into shared memory
    arraymemory = shared_memory.SharedMemory(create=True, size=offset_arr.nbytes)
    temp = np.ndarray(offset_arr.shape, dtype=offset_arr.dtype, buffer=arraymemory.buf)
    temp[:] = offset_arr[:]

    #Go over chunks
    with Pool(processes=number_processes) as pool:
        running_list = []
        for i in range(0,length,chunk):
            #Do not exceed number_processes
            if len(running_list) >= number_processes:
                running_list[0].wait()
            #Call a new function
            async_instance = pool.apply_async(return_string, \
            (stringmemory.name,arraymemory.name,offset_arr.shape,i))
            running_list.append(async_instance)
            #output
            if running_list[0].ready():
                write(1,running_list[0].get())
                del running_list[0]

        while len(running_list) != 0:
            running_list[0].wait()
            if running_list[0].ready():
                write(1,running_list[0].get())
                del running_list[0]

    stringmemory.close()
    stringmemory.unlink()
    arraymemory.close()
    arraymemory.unlink()

fizzbuzz(750000,int(1e100),32)
