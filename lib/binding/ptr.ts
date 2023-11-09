
/// the size_t(ype), i.e. representing the size (length) of a range of memory
export type size_t = number;

// pointers are not, and have never been, simple integers
// but also, they often have to become so at the point of use (no pun intended)
//
// all pointers have an implicit (contextual) range; very rarely it's  "the far
// end of memory" (e.g. the heap as seen from the stack, or vice versa) but much
// more commonly it's "the next 6 array elements" or so. So, this pointer carries
// both an address and a bytesLength to delineate that range.
//
// conceptually, that makes Ptr very similar to an array (which also represents a
// sized, contiguous slice of memory). For more on this duality, readers may be
// interested in Kernighan & Ritchie (1978).
//
export class Ptr {
	readonly data: DataView

	constructor(buffer: ArrayBufferLike & { BYTES_PER_ELEMENT?: never }, offset?: number, length?: number) {
		this.data = new DataView(buffer, offset, length)
	}

	get addr() {
		return this.data.byteOffset;
	}

	get byteLength() {
		return this.data.byteLength;
	}

	//        offset v
	//   [==================] ->
	//   [===========|======] ->
	// [
	//   [==========],
	//              [|======],
	// ]
	reslice(offset: size_t): [Ptr, Ptr] {
		return [
			this.slice(0, offset),
			this.slice(offset),
		]
	}

	/// Ptr.prototype.slice creates a new pointer that refers to a sub-range of the initial Ptr's memory from [start, end) (start included, end not included).
	/// Negative indices are interpreted relative to the end of the segment, i.e. ptr.slice(-1) is the last possible byte in ptr's range.
	///
	/// NB: because Ptr is a reference, and no underlying memory is copied, the slice can be thought of as "shallow": changes to the backing memory will be visible via a pointer and all its overlapping slices.
	/// This makes it very cheap to create (and pass), and effective for sharing views of the data to multiple consumers, and is directly comparable to Array.prototype.slice.
	/// However, this is starkly _distinct_ from `ArrayBuffer.prototype.slice`, which does perform a (deep) copy of the underlying data.
	///
	// TODO code examples demonstrating the problem Ptr solves by existing (trying to pass a reference into WebAssembly.Memory for a C++ function to be populated)
	slice(start?: number, end?: number) {
		start = start ?? 0;
		start = start < 0 ? this.byteLength + start : start;
		end = end ?? this.byteLength;
		end = end < 0 ? this.byteLength + end : end;
		const off = Math.min(this.addr + start, this.addr + this.byteLength);
		const len = Math.min(Math.max(0, end - start), this.byteLength);
		return new Ptr(this.data.buffer, off, len);
	}

	asRef() {
		return this.addr;
	}

	as<T>(ctor: new (buf: ArrayBufferLike, offset: number, length: number) => T): T {
		return new ctor(this.data.buffer, this.addr, this.byteLength)
	}

	// eh, we're all 32-bit for now anyway
	static get SIZE() {
		return 4;
	}
	getUsize(byteOffset: number, littleEndian?: boolean): number {
		return this.data.getUint32(byteOffset, littleEndian)
	}

	deref(byteOffset: number, size?: number, littleEndian?: boolean): Ptr {
		return new Ptr(this.data.buffer, this.getUsize(byteOffset, littleEndian), size)
	}

	// asArrayBuffer(_size?: size_t) {
	// 	throw new Error("todo")
	// 	// return this.data.slice(this.addr, size ? (this.addr + size) : undefined)
	// }
}

