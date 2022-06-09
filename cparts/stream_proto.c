// Stream protocol.

struct StreamIter {
	// the stream consumer may advance the data pointer and reduce
	// the remain count incrementally. when the remain count reaches
	// zero (or all remaining data has been consumed) the consumer
	// should call (*more) to request more data/space.
	byte* data;
	size_t remain;

	// call to request more data or more writing space.
	// data and remain may be in any state (callee should ignore them.)
	// callee may raise an exception or return bool success/fail.
	bool (*more)(StreamIter* iter);
}


// stream owners:
// pagebuf: { current page ptr, pagebuf ptr for adding pages }
// file: { fd or file pointer }

// requirements:
// pagebuf needs to keep state: current page ptr

// read-write iterator:
// writing may extend the underlying object.
// seeking and reading need to be aware of writes.

// some use cases:
// zlib needs to be passed buffer fragments on request.
// block read and write - e.g. pixel buffers.
// encoders write small byte groups.
// decoders read small runs of bytes - length unknown.


// six primitives:
// give me what you have - mem zero-copy or read buffer.
// read this many bytes - mem copy or read direct to dest.
// give me writing space - mem zero-copy or write buffer.
// write this data - mem copy or write direct from source.
// purge read buffers - ensure flushed data will be read.
// flush written data - commit any buffered writes.

// read-write interaction:
// when using the same iterator, purge/flush are unnecessary.
