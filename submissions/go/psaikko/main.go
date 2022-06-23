package main

import (
	"fmt"
	"io"
	"os"
	"runtime"
)

const limit = 1 << 61

func main() {
	initItoaCache()
	parallelFizzBuzz(1, limit)
}

const cacheSize = 10000

var logCacheSize = log10(cacheSize)
var itoaCache = make([]string, cacheSize)

func initItoaCache() {
	// precompute string representations
	fmtString := fmt.Sprintf("%%0%dd", logCacheSize)
	for j := 0; j < cacheSize; j++ {
		itoaCache[j] = fmt.Sprintf(fmtString, j)
	}
}

func parallelFizzBuzz(from, to int) {

	for _, wr := range getWidthRanges(from, to) {
		// range which can be filled with templates
		templatesStart := min(wr.to, ceilDiv(wr.from, templateLines)*templateLines)
		templatesEnd := templatesStart + floorDiv(wr.to-templatesStart+1, templateLines)*templateLines

		// handle values before first template
		for i := wr.from; i <= templatesStart; i++ {
			os.Stdout.WriteString(fizzBuzzLine(i))
		}

		// write large chunks in parallel
		const templatesPerJob = 250
		template, placeholderIdxs := fixedWidthTemplate(wr.width)
		nWorkers := runtime.NumCPU()
		chunkSize := nWorkers * templateLines * templatesPerJob

		chunksStart := templatesStart
		chunksEnd := chunksStart + floorDiv(templatesEnd-templatesStart+1, chunkSize)*chunkSize

		if chunksEnd > templatesStart {
			writeParallel(os.Stdout, chunksStart+1, chunksEnd, nWorkers, templatesPerJob, template, wr.width, placeholderIdxs)
		}

		// handle values after last chunk
		for i := chunksEnd + 1; i <= wr.to; i++ {
			os.Stdout.WriteString(fizzBuzzLine(i))
		}
	}
}

type widthRange struct{ from, to, width int }

// getWidthRanges splits integer range [from,to] into disjoint ranges grouped by base10 representation length
func getWidthRanges(from, to int) []widthRange {
	ranges := []widthRange{}

	fromWidth := log10(from + 1)
	toWidth := log10(to + 1)

	for fromWidth < toWidth {
		toVal := pow(10, fromWidth) - 1
		ranges = append(ranges, widthRange{from, toVal, fromWidth})
		from = toVal + 1
		fromWidth += 1
	}

	ranges = append(ranges, widthRange{from, to, fromWidth})
	return ranges
}

// fizzBuzzLine is used to form lines outside of "chunkable" regions
func fizzBuzzLine(i int) string {
	if (i%3 == 0) && (i%5 == 0) {
		return "FizzBuzz\n"
	} else if i%3 == 0 {
		return "Fizz\n"
	} else if i%5 == 0 {
		return "Buzz\n"
	} else {
		return fastItoa(uint64(i)) + "\n"
	}
}

const templateLines = 15

func fixedWidthTemplate(valueWidth int) ([]byte, []int) {
	template := make([]byte, 0, 15+valueWidth*8+4*8)
	formatString := fmt.Sprintf("%%0%dd\n", valueWidth)
	placeholder := []byte(fmt.Sprintf(formatString, 0))
	placeholderIdxs := make([]int, 0, 8)
	fizzBytes := []byte("Fizz\n")
	buzzBytes := []byte("Buzz\n")
	fizzBuzzBytes := []byte("FizzBuzz\n")

	placeholderIdxs = append(placeholderIdxs, len(template))
	template = append(template, placeholder...)
	placeholderIdxs = append(placeholderIdxs, len(template))
	template = append(template, placeholder...)
	template = append(template, fizzBytes...)
	placeholderIdxs = append(placeholderIdxs, len(template))
	template = append(template, placeholder...)
	template = append(template, buzzBytes...)
	template = append(template, fizzBytes...)
	placeholderIdxs = append(placeholderIdxs, len(template))
	template = append(template, placeholder...)
	placeholderIdxs = append(placeholderIdxs, len(template))
	template = append(template, placeholder...)
	template = append(template, fizzBytes...)
	template = append(template, buzzBytes...)
	placeholderIdxs = append(placeholderIdxs, len(template))
	template = append(template, placeholder...)
	template = append(template, fizzBytes...)
	placeholderIdxs = append(placeholderIdxs, len(template))
	template = append(template, placeholder...)
	placeholderIdxs = append(placeholderIdxs, len(template))
	template = append(template, placeholder...)
	template = append(template, fizzBuzzBytes...)
	return template, placeholderIdxs
}

func writeParallel(f io.Writer, firstLine, lastLine, nWorkers, templatesPerJob int, template []byte, width int, placeholderIdxs []int) {

	totalLines := lastLine - firstLine + 1
	workerLines := templateLines * templatesPerJob
	linesPerRound := nWorkers * workerLines
	if totalLines%linesPerRound != 0 {
		panic("uneven work allocation")
	}

	jobChannels := make([]chan int, nWorkers)
	resultChannels := make([]chan []byte, nWorkers)
	totalJobs := ceilDiv(totalLines, workerLines)
	jobsPerWorker := ceilDiv(totalLines, workerLines*nWorkers)

	for i := 0; i < nWorkers; i++ {
		jobChannels[i] = make(chan int, jobsPerWorker*2)
		resultChannels[i] = make(chan []byte, 1)
		go worker(jobChannels[i], resultChannels[i], templatesPerJob, template, width, placeholderIdxs)
	}

	// deal out jobs to workers
	for job := 0; job < totalJobs; job++ {
		jobLine := firstLine + job*workerLines
		jobChannels[job%nWorkers] <- jobLine
	}

	// read buffers from workers
	for job := 0; job < totalJobs; job++ {
		f.Write(<-resultChannels[job%nWorkers])
	}
}

func worker(in <-chan int, out chan<- []byte, templatesPerJob int, template []byte, width int, idxs []int) {
	buffer := make([]byte, len(template)*templatesPerJob)
	buffer2 := make([]byte, len(template)*templatesPerJob)
	buffer3 := make([]byte, len(template)*templatesPerJob)

	for i := 0; i < templatesPerJob; i++ {
		copy(buffer[len(template)*i:], template)
	}
	copy(buffer2, buffer)
	copy(buffer3, buffer)

	for jobLine := range in {

		nextFlush := (jobLine / cacheSize) * cacheSize
		repeat := false // need to fill twice for a clean template for cached ints

		for i := 0; i < templatesPerJob; i++ {
			off := i * len(template)
			if i*templateLines+jobLine+13 > nextFlush || repeat {
				bufFastItoa(buffer, off+idxs[0]+width, uint64(i*templateLines+jobLine))
				bufFastItoa(buffer, off+idxs[1]+width, uint64(i*templateLines+jobLine+1))
				bufFastItoa(buffer, off+idxs[2]+width, uint64(i*templateLines+jobLine+3))
				bufFastItoa(buffer, off+idxs[3]+width, uint64(i*templateLines+jobLine+6))
				bufFastItoa(buffer, off+idxs[4]+width, uint64(i*templateLines+jobLine+7))
				bufFastItoa(buffer, off+idxs[5]+width, uint64(i*templateLines+jobLine+10))
				bufFastItoa(buffer, off+idxs[6]+width, uint64(i*templateLines+jobLine+12))
				bufFastItoa(buffer, off+idxs[7]+width, uint64(i*templateLines+jobLine+13))
				repeat = !repeat
				if repeat {
					nextFlush += cacheSize
				}
			} else {
				copy(buffer[off:], buffer[off-len(template):off])
				copy(buffer[off+idxs[0]+width-logCacheSize:], itoaCache[(i*templateLines+jobLine)%cacheSize])
				copy(buffer[off+idxs[1]+width-logCacheSize:], itoaCache[(i*templateLines+jobLine+1)%cacheSize])
				copy(buffer[off+idxs[2]+width-logCacheSize:], itoaCache[(i*templateLines+jobLine+3)%cacheSize])
				copy(buffer[off+idxs[3]+width-logCacheSize:], itoaCache[(i*templateLines+jobLine+6)%cacheSize])
				copy(buffer[off+idxs[4]+width-logCacheSize:], itoaCache[(i*templateLines+jobLine+7)%cacheSize])
				copy(buffer[off+idxs[5]+width-logCacheSize:], itoaCache[(i*templateLines+jobLine+10)%cacheSize])
				copy(buffer[off+idxs[6]+width-logCacheSize:], itoaCache[(i*templateLines+jobLine+12)%cacheSize])
				copy(buffer[off+idxs[7]+width-logCacheSize:], itoaCache[(i*templateLines+jobLine+13)%cacheSize])
			}
		}

		out <- buffer
		buffer, buffer2, buffer3 = buffer2, buffer3, buffer
	}

	close(out)
}

//
// Helpers
//

func max(a, b int) int {
	if a < b {
		return b
	}
	return a
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func ceilDiv(val, divisor int) int {
	return (val + divisor - 1) / divisor
}

func floorDiv(val, divisor int) int {
	return val / divisor
}

// pow returns n^k
func pow(n, k int) int {
	if k == 0 {
		return 1
	} else if k == 1 {
		return n
	} else {
		return pow(n, k/2) * pow(n, k-k/2)
	}
}

// log10 returns base 10 logarithm for positive n
func log10(n int) int {
	if n <= 0 {
		panic("bad input")
	}
	i := 0
	c := 1
	for c < n {
		c *= 10
		i++
	}
	return i
}

const smallsString = "00010203040506070809" +
	"10111213141516171819" +
	"20212223242526272829" +
	"30313233343536373839" +
	"40414243444546474849" +
	"50515253545556575859" +
	"60616263646566676869" +
	"70717273747576777879" +
	"80818283848586878889" +
	"90919293949596979899"

// bufFastItoa writes base10 string representation of a positive integer to index i in buffer
// adapted from Go source strconv/itoa.go
func bufFastItoa(buf []byte, i int, u uint64) {

	for u >= 100 {
		is := u % 100 * 2
		u /= 100
		i -= 2
		buf[i+1] = smallsString[is+1]
		buf[i+0] = smallsString[is+0]
	}

	// u < 100
	is := u * 2
	i--
	buf[i] = smallsString[is+1]
	if u >= 10 {
		i--
		buf[i] = smallsString[is]
	}
}

// fastItoa returns base10 string representation of a positive integer
// adapted from Go source strconv/itoa.go
func fastItoa(u uint64) string {

	var dst [20]byte
	i := len(dst)

	for u >= 100 {
		is := u % 100 * 2
		u /= 100
		i -= 2
		dst[i+1] = smallsString[is+1]
		dst[i+0] = smallsString[is+0]
	}

	// u < 100
	is := u * 2
	i--
	dst[i] = smallsString[is+1]
	if u >= 10 {
		i--
		dst[i] = smallsString[is]
	}

	return string(dst[i:])
}
