from itertools import cycle, count

def derailleur(counter, carousel):
    if not carousel:
        return counter
    return carousel

def main():
    carousel = cycle([0,0,'Fizz',0,'Buzz','Fizz',0,0,'Fizz','Buzz',0,'Fizz',0,0,'FizzBuzz'])
    counter = count(1)
    f = map(print, map(derailleur, counter, carousel))
    while 1:
        next(f)

main()
