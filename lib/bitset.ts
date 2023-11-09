
// thanks to https://github.com/lemire/TypedFastBitSet.js

const shift = 6; // TODO relationship w/ BigUint64Array.BYTES_PER_ELEMENT /*8*/ + 1 ?

`
(module
	(func (export "i64.popcnt") (param i64) (result i32)
    local.get 0
    i64.popcnt
    i32.wrap_i64))
`
// via https://webassembly.github.io/wabt/demo/wat2wasm/
const wasm = (await
	WebAssembly.instantiateStreaming(
		fetch("data:application/wasm;base64,AGFzbQEAAAABBgFgAX4BfwMCAQAHDgEKaTY0LnBvcGNudAAACggBBgAgAHunCwAKBG5hbWUCAwEAAA=="),
		{}));

const instance = wasm.instance;
export const popcount = instance.exports['i64.popcnt'] as (v: bigint) => number;

interface Data {
	buffer: ArrayBufferLike,
	byteOffset: number,
	byteLength: number
}

// @ts-expect-error "duplicate identifier BitSet" (seems not to like my top-level await so much)
export class BitSet {
	words: BigUint64Array
	readonly growable: boolean

	constructor(iterable?: Iterable<number>, { data }: { data?: Data } = {}) {
		this.words = new BigUint64Array(data!.buffer, data!.byteOffset, data!.byteLength >> 3);
		// this.growable = (buffer === undefined);
		this.growable = false;

		for (const v of iterable ?? []) {
			this.add(v);
		}
	}

	asRef() {
		return this.words.byteOffset;
	}

	// 	constructor(iterable?: Iterable<number | bigint>, {
	// 		buffer?: ArrayBuffer,
	// 		maxElem: number = 12,
	// 	} = { }) {
	// 	// this.words = new BigUint64Array(buffer ?? new ArrayBuffer(maxElem >> (BigUint64Array.BYTES_PER_ELEMENT + BITS_PER_BYTE)))
	// 	this.words = new BigUint64Array();
	// }

	#grow(item: number) {
		const words = this.words;
		if (words.length << 6 > item) return;

		if (!this.growable)
			throw new Error("TODO message");

		const minWords = (item + 64) >>> shift;
		const sizeBytes = Math.max(minWords << 3, words.byteLength << 4);
		var arr = new BigUint64Array(new ArrayBuffer(sizeBytes));
		arr.set(this.words)
		this.words = arr
	}

	add(item: number) {
		this.#grow(item);

		const word = item >>> 6;
		const mask = 1n << BigInt(item & 0x3f);
		const val = this.words[word] ?? 0n;
		this.words[word] = val | mask;
		// TODO: `cannot mix BigInt and other types, use explicit conversions` ?
		// this.words[word]! |= BigInt(1n << item);
		return (val & mask) === 0n;
	}

	has(item: number) {
		const word = item >>> 6;
		const mask = 1n << BigInt(item & 0x3f);
		return ((this.words[word] ?? 0n) & mask) !== 0n;
	}

	size() {
		// TODO: efficiency
		return [...this].length;
	}

	[Symbol.iterator]() {
		// const iter = this.#cheat[Symbol.iterator]();
		const words = this.words;
		const c = words.length;
		let k = 0;
		let w = words[k]!;
		return {
			[Symbol.iterator]() {
				return this;
			},
			next() {
				while (k < c) {
					if (w !== 0n) {
						const t = w & -w;
						const value = (k << 6) + popcount(t - 1n);
						w ^= t;
						return { done: false, value };
					} else {
						k++;
						if (k < c) {
							w = words[k]!;
						}
					}
				}
				return { done: true, value: undefined };
			}
		}
	}

	clear() {
		this.words.fill(0n);
	}
}

// TODO
// 	const b = new TypedFastBitSet(); // initially empty
// b.add(1); // add the value "1"
// b.has(1); // check that the value is present! (will return true)
// b.add(2);
// console.log("" + b); // should display {1,2}
// b.add(10);
// b.addRange(11, 13);
// b.array(); // would return [1, 2, 10, 11, 12, 13]
// let c = new TypedFastBitSet([1, 2, 3, 10]); // create bitset initialized with values 1,2,3,10
// c.difference(b); // from c, remove elements that are in b (modifies c)
// c.difference2(b); // from c, remove elements that are in b (modifies b)
// c.change(b); // c will contain all elements that are in b or in c, but not both (elements that changed)
// const su = c.union_size(b); // compute the size of the union (bitsets are unchanged)
// c.union(b); // c will contain all elements that are in c and b
// const out1 = c.new_union(b); // creates a new bitmap that contains everything in c and b
// const out2 = c.new_intersection(b); // creates a new bitmap that contains everything that is in both c and b
// const out3 = c.new_change(b); // creates a new bitmap that contains everything in b or in c, but not both
// const s1 = c.intersection_size(b); // compute the size of the intersection (bitsets are unchanged)
// const s2 = c.difference_size(b); // compute the size of the difference (bitsets are unchanged)
// const s3 = c.change_size(b); // compute the number of elements that are in b but not c, or vice versa
// c.intersects(b); // return true if c intersects with b
// c.intersection(b); // c will only contain elements that are in both c and b
// c = b.clone(); // create a (deep) copy of b and assign it to c.
// c.equals(b); // checks whether c and b are equal
// c.forEach(fnc); // execute fnc on each value stored in c
// for (const x of c) fnc(x); // execute fnc on each value stored in c (allows early exit with break)
// c.trim(); // reduce the memory usage of the bitmap if possible, the content remains the same

