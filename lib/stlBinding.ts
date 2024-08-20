import { LITTLE_ENDIAN, type HasAbi, Ptr } from "./binding";

var Decoder = new TextDecoder('utf8');

// TODO: efficiently passing these things off to the DOM?
/// std::string is a std::basic_string<char>
export class std$$string {
	// a std::string has three mandatory features:
	// 1. a size
	// 2. a capacity
	// 3. some character data
	//
	// however; llvm's libc++ has an additional little bit of spice in its basic_string called "SSO": the "Small String Optimization."
	// For our purposes, what that means is that sometimes a std::string points to the character data, and sometimes it *is* the character data.
	// cf. https://stackoverflow.com/a/10319672/151464
	//
	// there's an additional wrinkle: the std::string ABI is sort-of stable, tied to _LIBCPP_ABI_VERSION (LIBCXX_ABI_VERSION ?)
	// cf. https://libcxx.llvm.org/DesignDocs/ABIVersioning.html
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 12;
	}

	// cf. https://github.com/llvm/llvm-project/blob/521ac12a251e7cf88d3b304186081de31a7503be/libcxx/include/string#L742-L759
	//
	// struct __long {
	//     pointer __data_;						          // size 4, offset 0
	//     size_type __size_;			              // size 4, offset 4
	//     size_type __cap_: 33, __is_long_: 1	// size 4, offset 8
	// }; 																			// total: 12
	//
	// struct __short {
	//     value_type __data_[11];	        		// size 11, offset 0
	//     // unsigned char __padding_[0];
	//     unsigned char
	// 					 __size_: 7,  __is_long_: 1;		// size 1, offset 11
	// }; 																			// total: 12

	get __is_long() {
		return !!(this.ptr.data.getUint8(11) & 0x80)
	}

	get data(): Ptr {
		return this.__is_long ? this.ptr.deref(0, this.length, LITTLE_ENDIAN) : this.ptr.slice(0, this.length);
	}

	get length() {
		return this.__is_long ? this.ptr.data.getUint32(4, LITTLE_ENDIAN) : (this.ptr.data.getUint8(11) & 0x7f);
	}

	get capacity() {
		return this.__is_long ? this.ptr.data.getUint32(8, LITTLE_ENDIAN) ^ 0x8000_0000 : 10;
	}


	asString(): string {
		return Decoder.decode(this.data.as(Uint8Array))
	}

	get [Symbol.toStringTag]() {
		return `std::string(${this.asString})`;
	}
}

// std::vector<T>
export function std$$vector(T: HasAbi) {
	return class {
		static get T() {
			return T;
		}
		static get SIZE(): number {
			return std$$vector.SIZE;
		}
		constructor(public ptr: Ptr) { }

		// ok so teknically there's no guarantee as to how a std::vector stores its state, and it sure could change at some point.
		// The way libc++ developers expect us to adopt that change is by re-compiling, which means `clang` looks at their snazzy new definition (that stores up to N elements inline or whatever) and recomputes all the various accessors and so on based on that new information.
		// This, obviously, is not being computed by clang based on the `libc++` defintion; but it is being computed by me (Seth) by looking at the definition used by the emscripten build.
		// Am I a good enough compiler to understand a pair[^1] of pointers and emulate clang's behavior? I guess we'll see!
		//
		// [^1]: well, triplet, but we're ignoring capacity (and allocators) for now.
		// cf. https://stackoverflow.com/a/52337100/151464
		get(idx: number) {
			const [start, end] = [
				this.ptr.deref(0 * Ptr.SIZE, /* size */ undefined, /* littleEndian */ true),
				this.ptr.deref(1 * Ptr.SIZE, /* size */ undefined, /* littleEndian */ true),
			];
			const elemSize = T.SIZE;
			const n = (end.addr - start.addr) / elemSize;
			if (idx < 0 || idx > n) {
				throw new Error(`out of bounds access: for index ${idx} with elements [0..${n})`);
			}
			const offset = idx * elemSize;
			return new T(
				start.slice(offset, offset + elemSize)
			);
		}

		[Symbol.iterator]() {
			const [start, end] = [
				this.ptr.deref(0 * Ptr.SIZE, /* size */ undefined /* defined by end */, /* littleEndian */ true),
				this.ptr.deref(1 * Ptr.SIZE, /* size */ 0, /* littleEndian */ true),
			];
			const elemSize = T.SIZE;
			var itr = start;
			return {
				next() {
					if (itr.addr >= end.addr) return { done: true, value: undefined as never };

					const ret = {
						done: false,
						value: new T(itr.slice(0, elemSize)),
					};
					itr = itr.slice(elemSize);
					return ret;
				},
			};
		}
	}
}
std$$vector.SIZE = 12;

// std::optional<T>
export function std$$optional(T: HasAbi) {
	return class {
		static get T() {
			return T;
		}
		static get SIZE(): number {
			console.assert(T == std$$string)
			// TODO this is actually the T-aligned value `T + 1`
			return 4 + T.SIZE;
		}
		constructor(public ptr: Ptr) { }

		// cf. https://github.com/llvm/llvm-project/blob/f49247129f3e873841bc6c3fec4bdc7c9d6f1dd7/libcxx/include/optional#L285-L289
		get __engaged_() {
			return !!(this.ptr.data.getUint8(T.SIZE) & 0x01)
		}

		deref() {
			if (!this.__engaged_) return undefined;
			return new T(this.ptr.slice(0, T.SIZE));
		}
	}
}

// std::deque<T> (aka std::queue<t>)
export function std$$deque(T: HasAbi) {
	return class {
		static get T() {
			return T;
		}
		static get SIZE(): number {
			return 24;
		}
		constructor(public ptr: Ptr) { }

		size() {
			return this.ptr.getUsize(20, LITTLE_ENDIAN);
		}
	}
}
std$$deque.SIZE = 24;
