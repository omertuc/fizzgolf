using System.Buffers;
using System.Runtime.CompilerServices;
using System.Threading.Channels;

var chan = Channel.CreateUnbounded<(byte[], int)>(new()
{
    SingleReader = true,
    SingleWriter = true
});

var tx = Task.Run(() =>
{
    var count = (nuint)0;
    while (true)
    {
        var buffer = ArrayPool<byte>.Shared.Rent(8 * 1024 * 1024);
        var span = buffer.AsSpan();
        var curr = 0;

        do
        {
            curr += WriteValue(span[curr..], count + 1);
            curr += WriteValue(span[curr..], count + 2);
            curr += WriteSpan(span[curr..], "Fizz"u8);
            curr += WriteValue(span[curr..], count + 4);
            curr += WriteSpan(span[curr..], "Buzz"u8);
            curr += WriteSpan(span[curr..], "Fizz"u8);
            curr += WriteValue(span[curr..], count + 7);
            curr += WriteValue(span[curr..], count + 8);
            curr += WriteSpan(span[curr..], "Fizz"u8);
            curr += WriteSpan(span[curr..], "Buzz"u8);
            curr += WriteValue(span[curr..], count + 11);
            curr += WriteSpan(span[curr..], "Fizz"u8);
            curr += WriteValue(span[curr..], count + 13);
            curr += WriteValue(span[curr..], count + 14);
            curr += WriteSpan(span[curr..], "FizzBuzz"u8);

            count += 15;
        } while (buffer.Length - curr >= 256);

        chan.Writer.TryWrite((buffer, curr));
    }
});

var rx = Task.Run(async () =>
{
    using var stdout = Console.OpenStandardOutput();
    await foreach (var (buffer, length) in chan.Reader.ReadAllAsync())
    {
        stdout.Write(buffer, 0, length);
        ArrayPool<byte>.Shared.Return(buffer);
    }
});

await Task.WhenAll(tx, rx);

[MethodImpl(MethodImplOptions.AggressiveInlining)]
static int WriteValue<T>(Span<byte> buffer, T value)
    where T : IUtf8SpanFormattable
{
    value.TryFormat(buffer, out var bytesWritten, default, null);
    buffer[bytesWritten] = (byte)'\n';
    return bytesWritten + 1;
}

[MethodImpl(MethodImplOptions.AggressiveInlining)]
static int WriteSpan(Span<byte> buffer, ReadOnlySpan<byte> span)
{
    span.CopyTo(buffer);
    buffer[span.Length] = (byte)'\n';
    return span.Length + 1;
}
