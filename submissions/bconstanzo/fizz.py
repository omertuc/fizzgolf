from itertools import cycle, count
from os import write

def derailleur(counter, carousel):
    return f"{carousel if carousel else counter}\n"

def main():
    carousel = cycle([0, 0, "Fizz", 0, "Buzz", "Fizz", 0, 0, "Fizz", "Buzz", 0, "Fizz", 0, 0, "FizzBuzz"])
    counter = count(1)
    f = map(derailleur, counter, carousel)
    while True:
        write(1,  "".join( [ next(f) for _ in range(8192) ] ).encode("utf-8") )

main()
