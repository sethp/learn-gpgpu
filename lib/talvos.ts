import { Ptr } from "./binding/ptr";
import { UTF8ArrayToString, stringToUTF8Array } from "./binding/strings";
import { std$$string } from "./stlBinding";

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
export class _EntryPoints {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 12;
	}

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
		const elemSize = Ptr.SIZE;
		const n = (end.addr - start.addr) / elemSize;
		if (idx < 0 || idx > n) {
			throw new Error(`out of bounds access: for index ${idx} with elements [0, ${n}]`);
		}
		return new Talvos$$EntryPoint(
			start.deref(idx * Ptr.SIZE, Talvos$$EntryPoint.SIZE, /* littleEndian */ true)
		);
	}

	[Symbol.iterator]() {
		const [start, end] = [
			this.ptr.deref(0 * Ptr.SIZE, /* size */ undefined /* defined by end */, /* littleEndian */ true),
			this.ptr.deref(1 * Ptr.SIZE, /* size */ 0, /* littleEndian */ true),
		];
		const elemSize = Ptr.SIZE;
		var itr = start;
		return {
			next() {
				if (itr.addr >= end.addr) return { done: true, value: undefined as never };

				const ret = {
					done: false,
					value: new Talvos$$EntryPoint(itr.deref(0, Talvos$$EntryPoint.SIZE, /* littleEndian */ true)),
				};
				itr = itr.slice(elemSize);
				return ret;
			},
		};
	}
}

export class Talvos$$Module {
	constructor(public ptr: Ptr) { }
	static get SIZE() {
		return 128;
	}

	get EntryPoints() {
		return new _EntryPoints(this.ptr.slice(40, 40 + _EntryPoints.SIZE));
	}
}
