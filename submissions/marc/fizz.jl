@inbounds function inc!(l::Vector{UInt8}, n, i = length(l)-1)
    if i > 0
        l[i] += n
        if l[i] > 0x39 # '9' 
            l[i] -= 0x0A # 10
            inc!(l, 0x01, i-1)
        end
    else
        pushfirst!(l, 0x31) # '1'
    end
    
    return nothing
end

function main(maxi = typemax(Int), N = 2^16)

    io = IOBuffer(; sizehint=N)
    a = UInt8['1', '\n']
    sizehint!(a, 100)

    for j in 1:N:maxi

        s = take!(io)
        write(stdout, s)
        l = ll = 0
        
        while ll+l+8 < N
            l = 0
            l += write(io, a)
            inc!(a, 0x01)
            l += write(io, a)
            l += write(io, "Fizz\n")
            inc!(a, 0x02)
            l += write(io, a)
            l += write(io, "Buzz\nFizz\n")
            inc!(a, 0x03)
            l += write(io, a)
            inc!(a, 0x01)
            l += write(io, a)
            l += write(io, "Fizz\nBuzz\n")
            inc!(a, 0x03)
            l += write(io, a)
            l += write(io, "Fizz\n")
            inc!(a, 0x02)
            l += write(io, a)
            inc!(a, 0x01)
            l += write(io, a)
            l += write(io, "FizzBuzz\n")
            inc!(a, 0x02)
            ll += l
        end
    end
end

main()
