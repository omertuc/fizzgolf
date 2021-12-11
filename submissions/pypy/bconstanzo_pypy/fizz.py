def fizzbuzz(x: int) -> str:
    if x % 15 == 0: return "FizzBuzz"
    if x % 5  == 0: return "Buzz"
    if x % 3  == 0: return "Fizz"
    return f"{x}"

def main():
    c = 0
    while True:
        print( "\n".join( fizzbuzz( c + i) for i in range(8192) ) )
        c += 8192

main()
