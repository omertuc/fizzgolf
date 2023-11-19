const PAGESIZE = 4096
const L2_CACHE_SIZE = 256 * PAGESIZE
const BUFSIZE = L2_CACHE_SIZE ÷ 2

"""
    ShortString("foo")

Represents a string that's short enough to fit entirely in a UInt128.
We take advantage of that by doing arithmetic on the UInt128 for
enumerating the decimal representation of the line numbers.
"""
struct ShortString
  val :: UInt128
  len :: Int
end

ShortString(s::String) = begin
  @assert length(s) <= sizeof(UInt128)
  s_padded = s * "\0" ^ sizeof(UInt128)
  val = unsafe_load(Ptr{UInt128}(pointer(s_padded)))
  ShortString(val, length(s))
end

Base.length(s::ShortString) = s.len

Base.:+(s::ShortString, x::Integer) = ShortString(s.val + x, s.len)
Base.:-(a::ShortString, b::ShortString) = begin
  @assert length(a) == length(b)
  a.val - b.val
end

concat(s::ShortString, a::Char) = begin
  newval = (s.val << 8) | UInt8(a)
  ShortString(newval, s.len + 1)
end

"""
    StaticBuffer(size)

Represents a simple byte array together with its next index.

This struct is non-mutable, and instead of updating `ptr` in place, we
replace it with a new StaticBuffer (see the `put` implementation).
This has experimentally been much faster; I think the compiler can apply
more optimizations when it keeps the struct on the stack.
"""
struct StaticBuffer
  buf :: Vector{UInt8}
  ptr :: Ptr{UInt128}
end

StaticBuffer(size) = begin
  buf = Vector{UInt8}(undef, size)
  ptr = pointer(buf)
  StaticBuffer(buf, ptr)
end

Base.length(buffer::StaticBuffer) = buffer.ptr - pointer(buffer.buf)
Base.pointer(buffer::StaticBuffer) = buffer.ptr
Base.truncate(buffer::StaticBuffer) = StaticBuffer(buffer.buf, pointer(buffer.buf))

put(buffer::StaticBuffer, s::ShortString) = begin
  unsafe_store!(buffer.ptr, s.val)
  StaticBuffer(buffer.buf, buffer.ptr + s.len)
end

almostfull(buffer::StaticBuffer) = begin
  length(buffer.buf) - (buffer.ptr - pointer(buffer.buf)) < PAGESIZE
end

"""
    withpipefd(f, io::IO, args...; kwds...)

Run `f` with a file descriptor (`::RawFD`) that is known to be a pipe; if `io`
isn't a pipe already, we insert a dummy `cat` process. This allows us to use
`vmsplice` which is much faster in the benchmark setup than `write`.
"""
withpipefd(f, io::Base.PipeEndpoint, args...; kwds...) = f(Base._fd(io), args...; kwds...)
withpipefd(f, io::Base.IOContext, args...; kwds...) = withpipefd(f, io.io, args...; kwds...)
withpipefd(f, io, args...; kwds...) = begin
  process = open(pipeline(`cat`, stdout=io), write=true)
  withpipefd(f, process.in, args...; kwds...)
  close(process)
end

"""
    vmsplice(fdesc, buffer)

Splice the data in `buffer` to the pipe in `fdesc`.
"""
vmsplice(fdesc::RawFD, buffer::StaticBuffer) = begin
  ptr = pointer(buffer.buf)
  while ptr < buffer.ptr
    written = @ccall vmsplice(
      fdesc :: Cint,
      (ptr, buffer.ptr - ptr) :: Ref{Tuple{Ref{UInt8}, Csize_t}},
      1 :: Csize_t,
      0 :: Cuint) :: Cssize_t
    if written < 0
      error("Couldn't write to pipe")
    end
    ptr += written
  end
end

"""
    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)

Move asciidigits and intdigits to the next line, i.e. add 1
to the ascii and decimal representations.
"""
@inline nextline(asciidigits, intdigits, plusone) = begin
  asciidigits += plusone
  intdigits = Base.setindex(intdigits, intdigits[1] + 1, 1)
  asciidigits, intdigits
end

const CARRY = ShortString("20") - ShortString("1:")

"""
    asciidigits, plusone, pluscarry = carry(position, asciidigits, plusone, pluscarry)

Perform a carry operation on asciidigits in the `position`th decimal place.
"""
@inline carry(position, asciidigits, plusone, pluscarry) = begin
  if position + 1 == length(asciidigits)
    asciidigits = concat(asciidigits, '0')

    plusone <<= 8
    pluscarry = pluscarry .<< 8
    pluscarry = Base.setindex(pluscarry, CARRY, position)
  end
  asciidigits += pluscarry[position]
  asciidigits, plusone, pluscarry
end

"""
    @compiletime for a in b
      <statements>
    end

Unroll the loop.
"""
macro compiletime(forloop)
  @assert forloop.head == :for
  it, body = forloop.args
  @assert it.head == :(=)
  lhs, rhs = it.args

  expressions = gensym(:expressions)

  body = quote
    push!($expressions, $(Expr(:quote, body)))
  end

  expressions = Core.eval(__module__, quote
    let $expressions = []
      for $lhs in $rhs
        $body
      end
      $expressions
    end
  end)

  return esc(quote
    $(expressions...)
  end)
end

"""
    asciidigits, intdigits, plusone, pluscarry = maybecarry(asciidigits, intdigits, plusone, pluscarry)

If necessary, perform a carry operation on asciidigits and intdigits.
"""
@inline maybecarry(asciidigits, intdigits, plusone, pluscarry) = begin
  asciidigits += plusone

  @compiletime for d in 1:16
    intdigits = Base.setindex(intdigits, intdigits[$d] + 1, $d)
    intdigits[$d] != 10 && @goto carried
    intdigits = Base.setindex(intdigits, 0, $d)
    asciidigits, plusone, pluscarry = carry($d, asciidigits, plusone, pluscarry)
  end

  intdigits = Base.setindex(intdigits, intdigits[17] + 1, 17)
  intdigits[17] >= 10 && error("too big!")

  @label carried
  asciidigits, intdigits, plusone, pluscarry
end

const FIZZ = ShortString("Fizz\n")
const BUZZ = ShortString("Buzz\n")
const FIZZBUZZ = ShortString("FizzBuzz\n")

initialstate() = (
  intdigits = (1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
  asciidigits = ShortString("1\n"),
  plusone = UInt128(1),
  pluscarry = ntuple(_ -> zero(UInt128), Val(sizeof(UInt128)))
)

fizzbuzz(buffer::StaticBuffer, state) = begin
  (;intdigits, asciidigits, plusone, pluscarry) = state

  while !almostfull(buffer)
    buffer = put(buffer, asciidigits)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, asciidigits)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, FIZZ)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, asciidigits)

    asciidigits, intdigits, plusone, pluscarry = maybecarry(asciidigits, intdigits, plusone, pluscarry)
    buffer = put(buffer, BUZZ)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, FIZZ)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, asciidigits)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, asciidigits)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, FIZZ)

    asciidigits, intdigits, plusone, pluscarry = maybecarry(asciidigits, intdigits, plusone, pluscarry)
    buffer = put(buffer, BUZZ)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, asciidigits)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, FIZZ)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, asciidigits)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
    buffer = put(buffer, asciidigits)

    asciidigits, intdigits, plusone, pluscarry = maybecarry(asciidigits, intdigits, plusone, pluscarry)
    buffer = put(buffer, FIZZBUZZ)

    asciidigits, intdigits = nextline(asciidigits, intdigits, plusone)
  end
  buffer, (;intdigits,asciidigits,plusone,pluscarry)
end

fizzbuzz(fdesc::RawFD, cutoff=typemax(Int)) = begin
  pipesize = @ccall fcntl(fdesc::Cint, 1031::Cint, BUFSIZE::Cint)::Cint
  @assert pipesize == BUFSIZE

  buf1, buf2 = StaticBuffer(BUFSIZE), StaticBuffer(BUFSIZE)

  state = initialstate()
  n = 0

  @GC.preserve buf1 buf2 while n <= cutoff

    buf1, state = fizzbuzz(truncate(buf1), state)
    vmsplice(fdesc, buf1)
    n += length(buf1)


    buf2, state = fizzbuzz(truncate(buf2), state)
    vmsplice(fdesc, buf2)
    n += length(buf2)
  end
end

"""
  fizzbuzz(io::IO, cutoff=typemax(Int))

Write the fizzbuzz output to `io`.

The `cutoff` parameter is approximate; depending on buffering, more bytes
may be written to `io`.
"""
fizzbuzz(io::IO, cutoff=typemax(Int)) = withpipefd(fizzbuzz, io, cutoff)

if abspath(PROGRAM_FILE) == @__FILE__
  fizzbuzz(stdout)
end
