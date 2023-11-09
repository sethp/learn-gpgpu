export var lengthBytesUTF8 = (str: string) => {
	var len = 0;
	for (var i = 0; i < str.length; ++i) {
		var c = str.charCodeAt(i); // possibly a lead surrogate
		if (c <= 0x7F) {
			len++;
		} else if (c <= 0x7FF) {
			len += 2;
		} else if (c >= 0xD800 && c <= 0xDFFF) {
			len += 4; ++i;
		} else {
			len += 3;
		}
	}
	return len;
};

// TODO
// type buffer = ArrayBufferLike;
type buffer = any;

var UTF8Encoder = new TextEncoder();
export var stringToUTF8Array = (str: string, heap: buffer, outIdx: number, maxBytesToWrite: number) => {
	// assert(typeof str === 'string');
	// Parameter maxBytesToWrite is not optional. Negative values, 0, null,
	// undefined and false each don't write out any bytes.
	if (!(maxBytesToWrite > 0))
		return 0;

	if (str.length > 16) {
		const { written } = UTF8Encoder.encodeInto(str, heap.subarray(outIdx, maxBytesToWrite))
		return written;
	}

	var startIdx = outIdx;
	var endIdx = outIdx + maxBytesToWrite - 1; // -1 for string null terminator.
	for (var i = 0; i < str.length; ++i) {
		// cf. https://www.unicode.org/glossary/#code_unit
		// vs. https://www.unicode.org/glossary/#code_point
		// charCodeAt returns a 16-bit code _unit_, not code _point_
		// for our purposes, the difference is that the

		// So translate UTF16->UTF32->UTF8.
		// See http://unicode.org/faq/utf_bom.html#utf16-3
		// For UTF8 byte structure, see http://en.wikipedia.org/wiki/UTF-8#Description
		// and https://www.ietf.org/rfc/rfc2279.txt
		// and https://tools.ietf.org/html/rfc3629
		var u = str.charCodeAt(i);
		if (u >= 0xD800 && u <= 0xDFFF) {
			var u1 = str.charCodeAt(++i);
			u = 0x10000 + ((u & 0x3FF) << 10) | (u1 & 0x3FF);
		}
		if (u <= 0x7F) {
			if (outIdx >= endIdx) break;
			heap[outIdx++] = u;
		} else if (u <= 0x7FF) {
			if (outIdx + 1 >= endIdx) break;
			heap[outIdx++] = 0xC0 | (u >> 6);
			heap[outIdx++] = 0x80 | (u & 63);
		} else if (u <= 0xFFFF) {
			if (outIdx + 2 >= endIdx) break;
			heap[outIdx++] = 0xE0 | (u >> 12);
			heap[outIdx++] = 0x80 | ((u >> 6) & 63);
			heap[outIdx++] = 0x80 | (u & 63);
		} else {
			if (outIdx + 3 >= endIdx) break;
			// TODO replacement characters or something?
			if (u > 0x10FFFF) throw new Error('Invalid Unicode code point')
			heap[outIdx++] = 0xF0 | (u >> 18);
			heap[outIdx++] = 0x80 | ((u >> 12) & 63);
			heap[outIdx++] = 0x80 | ((u >> 6) & 63);
			heap[outIdx++] = 0x80 | (u & 63);
		}
	}
	// Null-terminate the pointer to the buffer.
	heap[outIdx] = 0;
	return outIdx - startIdx;
};


// var stringToUTF8 = (str, outPtr, maxBytesToWrite) => {
// 	assert(typeof maxBytesToWrite == 'number', 'stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!');
// 	return stringToUTF8Array(str, HEAPU8, outPtr, maxBytesToWrite);
// };
// var stringToUTF8OnStack = (str) => {
// 	var size = lengthBytesUTF8(str) + 1;
// 	var ret = stackAlloc(size);
// 	stringToUTF8(str, ret, size);
// 	return ret;
// };

// export function strnlen(buf: buffer, maxLen: number) {
// }

var UTF8Decoder = typeof TextDecoder != 'undefined' ? new TextDecoder('utf8') : undefined;

export var UTF8ArrayToString = (heapOrArray: buffer, idx: number, maxBytesToRead: number, knownLength?: number): string => {
	const endIdx = idx + maxBytesToRead;
	// TextDecoder needs to know the byte length in advance, it doesn't stop on
	// null terminator by itself.  Also, use the length info to avoid running tiny
	// strings through TextDecoder, since .subarray() allocates garbage.
	const endPtr = knownLength ? idx + knownLength : (() => {
		let endPtr = idx;
		// (As a tiny code save trick, compare endPtr against endIdx using a negation,
		// so that undefined means Infinity)
		while (heapOrArray[endPtr] && !(endPtr >= endIdx)) ++endPtr;
		return endPtr;
	})()

	if (endPtr - idx > 16 && heapOrArray.buffer && UTF8Decoder) {
		return UTF8Decoder.decode(heapOrArray.subarray(idx, endPtr));
	}
	var str = '';
	// If building with TextDecoder, we have already computed the string length
	// above, so test loop end condition against that
	while (idx < endPtr) {
		// For UTF8 byte structure, see:
		// http://en.wikipedia.org/wiki/UTF-8#Description
		// https://www.ietf.org/rfc/rfc2279.txt
		// https://tools.ietf.org/html/rfc3629
		var u0 = heapOrArray[idx++];
		if (!(u0 & 0x80)) { str += String.fromCharCode(u0); continue; }
		var u1 = heapOrArray[idx++] & 63;
		if ((u0 & 0xE0) == 0xC0) { str += String.fromCharCode(((u0 & 31) << 6) | u1); continue; }
		var u2 = heapOrArray[idx++] & 63;
		if ((u0 & 0xF0) == 0xE0) {
			u0 = ((u0 & 15) << 12) | (u1 << 6) | u2;
		} else {
			// TODO replacement characters or something?
			if ((u0 & 0xF8) != 0xF0) throw new Error('Invalid Unicode code point')
			u0 = ((u0 & 7) << 18) | (u1 << 12) | (u2 << 6) | (heapOrArray[idx++] & 63);
		}

		if (u0 < 0x10000) {
			str += String.fromCharCode(u0);
		} else {
			var ch = u0 - 0x10000;
			str += String.fromCharCode(0xD800 | (ch >> 10), 0xDC00 | (ch & 0x3FF));
		}
	}
	return str;
};

// /**
//  * Given a pointer 'ptr' to a null-terminated UTF8-encoded string in the
//  * emscripten HEAP, returns a copy of that string as a Javascript String object.
//  *
//  * @param {number} ptr
//  * @param {number=} maxBytesToRead - An optional length that specifies the
//  *   maximum number of bytes to read. You can omit this parameter to scan the
//  *   string until the first 0 byte. If maxBytesToRead is passed, and the string
//  *   at [ptr, ptr+maxBytesToReadr[ contains a null byte in the middle, then the
//  *   string will cut short at that byte index (i.e. maxBytesToRead will not
//  *   produce a string of exact length [ptr, ptr+maxBytesToRead[) N.B. mixing
//  *   frequent uses of UTF8ToString() with and without maxBytesToRead may throw
//  *   JS JIT optimizations off, so it is worth to consider consistently using one
//  * @return {string}
//  */
// var UTF8ToString = (ptr, maxBytesToRead) => {
// 	assert(typeof ptr == 'number');
// 	return ptr ? UTF8ArrayToString(HEAPU8, ptr, maxBytesToRead) : '';
// };
