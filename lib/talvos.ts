import { eagerDeref } from "./binding";
import { Ptr } from "./binding/ptr";
import { UTF8ArrayToString, stringToUTF8Array } from "./binding/strings";
import { std$$deque, std$$optional, std$$string, std$$vector } from "./stlBinding";

const LITTLE_ENDIAN = true; // yay booleans

const NULLPTR = 0;

export class Talvos$$Params {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 64;
	}

	get EntryName() {
		return UTF8ArrayToString(this.ptr.slice(0, 64).as(Uint8Array), 0, 64);
	}
	set EntryName(name: string) {
		const written = stringToUTF8Array(name, this.ptr.slice(0, 64).as(Uint8Array), 0, 64);
		if (written < name.length) {
			throw new Error('out of memory: string too long');
		}
	}
}

export class Talvos$$EntryPoint {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 36;
	}

	get Name() {
		return new std$$string(this.ptr.slice(4, 4 + std$$string.SIZE));
	}
}

// std::vector<talvos::EntryPoint *>
// --> Iterable<Talvos$$EntryPoint>
export class _EntryPoints extends std$$vector(eagerDeref(Talvos$$EntryPoint)) {
}

export class Talvos$$Buffer {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 32;
	}

	get Id() {
		return this.ptr.data.getUint32(0, LITTLE_ENDIAN);
	}

	get Size() {
		return this.ptr.data.getUint32(8, LITTLE_ENDIAN);
	}

	get Name() {
		const rTy = std$$optional(std$$string);
		return new (rTy)(this.ptr.slice(16, 16 + rTy.SIZE));
	}
}

export class Talvos$$Module {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 128;
	}

	get Buffers() {
		return new (std$$vector(Talvos$$Buffer))(this.ptr.slice(116, 116 + std$$vector.SIZE));
	}

	get EntryPoints() {
		return new _EntryPoints(this.ptr.slice(40, 40 + _EntryPoints.SIZE));
	}
}

class Talvos$$Memory$$Alloc {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 16;
	}

	get NumBytes(): bigint {
		return this.ptr.data.getBigUint64(0, LITTLE_ENDIAN);
	}

	get Data(): Ptr {
		return this.ptr.deref(8, Number(this.NumBytes), LITTLE_ENDIAN)
	}
}

export class Talvos$$Memory {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 2456;
	}

	// TODO[seth]: it would be nice, at some point, to think about how to bind this to the C++ impl
	// (unfortunately, that means making the whole enchilada async, don't it?)
	// is there any way to late-bind the method impls?

	// TODO[seth]: wrap device-side addresses in a type? and/or distinguish device-side `Ptr`s somehow?
	// uh oh, number isn't precise enough lololol; I guess that solves that (kinda)

	// cf. `Memory::load` (which `memcpy`s the data)
	deref(Addr: bigint, Size: number): Ptr {
		const BUFFER_BITS = 16n;
		const OFFSET_BITS = 48n;

		const Id = Addr >> OFFSET_BITS;
		const Offset = Number(Addr & BigInt.asUintN(64, (2n ** 64n - 1n) >> BUFFER_BITS));

		const Alloc = this.Allocs.get(Number(Id));
		console.assert(Offset + Size <= Alloc.NumBytes)

		return Alloc.Data.slice(Offset, Size);
	}

	private get Allocs() {
		return new (std$$vector(Talvos$$Memory$$Alloc))(this.ptr.slice(2432, 2432 + std$$vector.SIZE))
	}

}

export class Talvos$$Object {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 20;
	}

	get Data(): Ptr {
		return this.ptr.deref(4, undefined /* TODO the type knows */, LITTLE_ENDIAN)
	}
}

export class Talvos$$PipelineExecutor {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 272;
	}

	get Objects() {
		return new (std$$vector(Talvos$$Object))(this.ptr.slice(32, 32 + std$$vector.SIZE))
	}
}

export class Talvos$$Device {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 112;
	}

	get GlobalMemory() {
		return new Talvos$$Memory(this.ptr.deref(16, Talvos$$Memory.SIZE, LITTLE_ENDIAN));
	}

	get PipelineExecutor() {
		return new Talvos$$PipelineExecutor(this.ptr.deref(32, Talvos$$PipelineExecutor.SIZE, LITTLE_ENDIAN));
	}
}

export class Talvos$$Dim3 {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 12;
	}

	get X() { return this.ptr.data.getUint32(0, LITTLE_ENDIAN); }
	get Y() { return this.ptr.data.getUint32(4, LITTLE_ENDIAN); }
	get Z() { return this.ptr.data.getUint32(8, LITTLE_ENDIAN); }

	// fixme: this could be a CArray type or something in lib/binding
	get(idx: number) {
		const elemSize = 4;
		const n = 3;
		if (idx < 0 || idx > n) {
			throw new Error(`out of bounds access: for index ${idx} with elements [0..${n})`);
		}
		const offset = idx * elemSize;
		return this.ptr.data.getUint32(offset, LITTLE_ENDIAN);
	}

	[Symbol.iterator]() {
		const [start, end] = this.ptr.reslice(Talvos$$Dim3.SIZE);
		const elemSize = 4;
		var itr = start;
		return {
			next() {
				if (itr.addr >= end.addr) return { done: true, value: undefined as never };

				const ret = {
					done: false,
					value: itr.data.getUint32(0, LITTLE_ENDIAN),
				};
				itr = itr.slice(elemSize);
				return ret;
			},
		};
	}

}

export class Talvos$$Instruction {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 20;
	}

	// hmm, nulls.
	// TODO: what's at the very beginning of memory? can we ban pointers to it at construction?
	hasResultType() { return this.ptr.getUsize(0, LITTLE_ENDIAN) != NULLPTR; }
	get ResultType() {
		return new Talvos$$Type(this.ptr.deref(0, Talvos$$Type.SIZE, LITTLE_ENDIAN));
	}

	get Opcode() {
		return this.ptr.data.getUint16(4, LITTLE_ENDIAN);
	}

	get NumOperands() {
		return this.ptr.data.getUint16(6, LITTLE_ENDIAN);
	}

	get Operands() {
		// fixme: this could be a SizedCArray type or something in lib/binding
		const elemSize = 4;
		const n = this.NumOperands;
		const ptr = this.ptr.deref(8, elemSize * n, LITTLE_ENDIAN);
		return new class {
			get(idx: number) {
				if (idx < 0 || idx > n) {
					throw new Error(`out of bounds access: for index ${idx} with elements [0..${n})`);
				}
				const offset = idx * elemSize;
				return ptr.data.getUint32(offset, LITTLE_ENDIAN);
			}

			[Symbol.iterator]() {
				const [start, end] = ptr.reslice(ptr.byteLength);
				var itr = start;
				return {
					next() {
						if (itr.addr >= end.addr) return { done: true, value: undefined as never };

						const ret = {
							done: false,
							value: itr.data.getUint32(0, LITTLE_ENDIAN),
						};
						itr = itr.slice(elemSize);
						return ret;
					},
				};
			}
		}
	}
}

export class Talvos$$Type {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 88;
	}
}

export class Talvos$$Core {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 28;
	}

	get Microtasks() {
		return new (std$$deque(undefined as any))(this.ptr.slice(4, 4 + std$$deque.SIZE))
	}

	get PC() {
		// nullable
		let tgt = this.ptr.deref(0, Talvos$$Instruction.SIZE, LITTLE_ENDIAN);
		return tgt.addr == 0 ? null : new Talvos$$Instruction(tgt);
	}
}

export class Talvos$$Cores extends std$$vector(Talvos$$Core) { }
