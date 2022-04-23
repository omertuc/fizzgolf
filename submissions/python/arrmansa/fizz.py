def fizzbuzz(chunk,length):
    fb_string = ""
    for i in range(0,chunk):
        if i%15 == 0: fb_string += "FizzBuzz"
        elif i%3 == 0: fb_string += "Fizz"
        elif i%5 == 0: fb_string += "Buzz"
        else: fb_string += "%i"
        fb_string += "\n"
    offset_tuple = tuple(i for i in range(chunk) if i%3 != 0 and i%5 != 0)
    for i in range(0,length,chunk):
        print(fb_string % offset_tuple, end='')
        offset_tuple = tuple(i + chunk for i in offset_tuple)
fizzbuzz(6000,int(1e100))
