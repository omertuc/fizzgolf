package main

import (
    "os"
    "strconv"
    "sync"
    "unsafe"
)

const buffSize = 200000000
const innerloop = 25000

const routines = 12

func main() {
    eightZeroLock := &sync.Mutex{}
    startLock := eightZeroLock
    endLock := &sync.Mutex{}
    endLock.Lock()
    for i := 0; i < routines-1; i++ {
        go routine(i, startLock, endLock)
        startLock = endLock
        endLock = &sync.Mutex{}
        endLock.Lock()
    }
    go routine(routines-1, startLock, eightZeroLock)
    wg := sync.WaitGroup{}
    wg.Add(1)
    wg.Wait()
}

func routine(num int, start, end *sync.Mutex) {
    counter := num * 15 * innerloop

    var sb Builder
    sb.Grow(buffSize)
    for {

        for i := 0; i < innerloop; i++ {
            sb.WriteString(strconv.Itoa(counter + 1))
            sb.WriteString("\n")
            sb.WriteString(strconv.Itoa(counter + 2))
            sb.WriteString("\nFizz\n")
            sb.WriteString(strconv.Itoa(counter + 4))
            sb.WriteString("\nBuzz\nFizz\n")
            sb.WriteString(strconv.Itoa(counter + 7))
            sb.WriteString("\n")
            sb.WriteString(strconv.Itoa(counter + 8))
            sb.WriteString("\nFizz\nBuzz\n")
            sb.WriteString(strconv.Itoa(counter + 11))
            sb.WriteString("\nFizz\n")
            sb.WriteString(strconv.Itoa(counter + 13))
            sb.WriteString("\n")
            sb.WriteString(strconv.Itoa(counter + 14))
            sb.WriteString("\nFizzBuzz\n")

            counter += 15
        }
        start.Lock()
        os.Stdout.WriteString((sb.String()))
        end.Unlock()
        sb.buf = sb.buf[:0]
        counter += 15 * (routines - 1) * innerloop

    }
}

// After this copied from go stringBuilder(some lines removed)
// Copyright 2017 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// A Builder is used to efficiently build a string using Write methods.
// It minimizes memory copying. The zero value is ready to use.
// Do not copy a non-zero Builder.
type Builder struct {
    addr *Builder // of receiver, to detect copies by value
    buf  []byte
}

// noescape hides a pointer from escape analysis.  noescape is
// the identity function but escape analysis doesn't think the
// output depends on the input. noescape is inlined and currently
// compiles down to zero instructions.
// USE CAREFULLY!
// This was copied from the runtime; see issues 23382 and 7921.
//go:nosplit
//go:nocheckptr
func noescape(p unsafe.Pointer) unsafe.Pointer {
    x := uintptr(p)
    return unsafe.Pointer(x ^ 0)
}

func (b *Builder) copyCheck() {
    if b.addr == nil {
        // This hack works around a failing of Go's escape analysis
        // that was causing b to escape and be heap allocated.
        // See issue 23382.
        // TODO: once issue 7921 is fixed, this should be reverted to
        // just "b.addr = b".
        b.addr = (*Builder)(noescape(unsafe.Pointer(b)))
    } else if b.addr != b {
        panic("strings: illegal use of non-zero Builder copied by value")
    }
}

// String returns the accumulated string.
func (b *Builder) String() string {
    return *(*string)(unsafe.Pointer(&b.buf))
}

// grow copies the buffer to a new, larger buffer so that there are at least n
// bytes of capacity beyond len(b.buf).
func (b *Builder) grow(n int) {
    buf := make([]byte, len(b.buf), 2*cap(b.buf)+n)
    copy(buf, b.buf)
    b.buf = buf
}

// Grow grows b's capacity, if necessary, to guarantee space for
// another n bytes. After Grow(n), at least n bytes can be written to b
// without another allocation. If n is negative, Grow panics.
func (b *Builder) Grow(n int) {
    if n < 0 {
        panic("strings.Builder.Grow: negative count")
    }
    if cap(b.buf)-len(b.buf) < n {
        b.grow(n)
    }
}

// WriteString appends the contents of s to b's buffer.
// It returns the length of s and a nil error.
func (b *Builder) WriteString(s string) (int, error) {
    b.buf = append(b.buf, s...)
    return len(s), nil
}
