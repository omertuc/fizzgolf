from os import write

buf = bytearray(256)
out = bytearray(65536 + 4096)

init = b"1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nFizz\n"

out[:len(init)] = init

fmt = "Buzz\n{}1\nFizz\n{}3\n{}4\nFizzBuzz\n{}6\n{}7\nFizz\n{}9\nBuzz\nFizz\n{}2\n{}3\nFizz\nBuzz\n{}6\nFizz\n{}8\n{}9\nFizzBuzz\n{}1\n{}2\nFizz\n{}4\nBuzz\nFizz\n{}7\n{}8\nFizz\n"

t = 30
i = 1
j = 1

for l in range(1, 20):
    txt = fmt.format(i, i, i, i, i, i, i + 1, i + 1, i + 1, i + 1, i + 1, i + 2, i + 2, i + 2, i + 2, i + 2).encode()
    buf[:len(txt)] = txt

    i *= 10
    while j < i:
        out[t:t+len(txt)] = buf[:len(txt)]
        t += len(txt)
        if t >= 65536:
            u = write(1, out[:65536])
            while u < 65536:
                u += write(1, out[u:65536])

            t -= 65536
            out[:t] = out[65536:65536+t]

        q = 0
        for z in (4, 7, 2, 11, 2, 7, 12, 2, 12, 7, 2, 11, 2, 7, 12, 2):
            q += z + l
            p = q
            if buf[p] < 55:
                buf[p] += 3
            else:
                buf[p] -= 7
                p -= 1
                while buf[p] == 57:
                    buf[p] = 48
                    p -= 1

                buf[p] += 1

        j += 3
