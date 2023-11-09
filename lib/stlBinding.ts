import type { Ptr } from "./binding/ptr";

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
		return this.__is_long ? this.ptr.deref(0, this.length, true) : this.ptr.slice(0, this.length);
	}

	get length() {
		return this.__is_long ? this.ptr.data.getUint32(4, true) : (this.ptr.data.getUint8(11) & 0x7f);
	}

	get capacity() {
		return this.__is_long ? this.ptr.data.getUint32(8, true) ^ 0x8000_0000 : 10;
	}


	asString(): string {
		return Decoder.decode(this.data.as(Uint8Array))
	}

	get [Symbol.toStringTag]() {
		return `std::string(${this.asString})`;
	}
}
