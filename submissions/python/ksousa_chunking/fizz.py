from itertools import cycle, count

def derailleur(counter, carousel):
    if not carousel:
        return counter
    return carousel

def main():
    carousel = cycle([0,0,'Fizz',0,'Buzz','Fizz',0,0,'Fizz','Buzz',0,'Fizz',0,0,'FizzBuzz'])
    counter = map(str, count(1))
    f = map(derailleur, counter, carousel)
    while 1:
        print('\n'.join([next(f) for _ in range(256)]))

main()
