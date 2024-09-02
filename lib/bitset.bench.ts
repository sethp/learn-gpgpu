import { bench, expect, suite } from 'vitest';
import { BitSet, popcount } from './bitset';

suite('empty', () => {
	bench("baseline", () => {
	})
})

suite("BitSet.size", async () => {


	// suite.only("size", async () => {
	// const vals = [1, 2, 3, 4, 5, 31];
	// const vals = [1, 2, 3, 4, 5, 4097];
	const vals = [1, 2, 3, 4, 5, 163357];

	const b = new BitSet(vals);

	bench("baseline", () => {
		if (b.size() != 6) throw new Error("oops")
	})

	bench("loop", () => {
		const nw = b.words.length;
		let size = 0;
		let k = 0 | 0;
		for (; k < nw; ++k) {
			size += popcount(b.words[k]!);
		}

		if (size != 6) throw new Error("oops")
	})

	bench("unrolled", () => {
		const words = b.words;
		const nw = words.length;
		let size = 0;
		let k = 0 | 0;
		for (; k + 4 < nw; k += 4) {
			size += popcount(words[k]!);
			size += popcount(words[k + 1]!);
			size += popcount(words[k + 2]!);
			size += popcount(words[k + 3]!);
		}

		for (; k < nw; ++k) {
			size += popcount(words[k]!);
		}


		if (size != 6) throw new Error("oops")
	})

	// bench("unrolled_fake", () => {
	// 	const words = b.words;
	// 	const nw = words.length;
	// 	let size = 0;
	// 	let k = 0 | 0;
	// 	for (; k + 4 < nw; k += 4)
	// 		size += (k == 0 ? 5 : 0);

	// 	for (; k < nw; ++k) {
	// 		size += popcount(words[k]!);
	// 	}


	// 	if (size != 6) throw new Error("oops")
	// })

	bench("wasm_v128", () => {
		const words = b.words;
		const nw = words.length;
		let size = 0;
		let k = 0 | 0;
		for (; k + 2 < nw; k += 2)
			size += popcount_wasm_v128(words[k]!, words[k + 1]!);

		for (; k < nw; ++k) {
			size += popcount(words[k]!);
		}


		if (size != 6) throw new Error("oops")
	})

	bench("wasm_2xi64", () => {
		const words = b.words;
		const nw = words.length;
		let size = 0;
		let k = 0 | 0;
		for (; k + 2 < nw; k += 2)
			size += popcount_wasm_2xi64(words[k]!, words[k + 1]!);

		for (; k < nw; ++k) {
			size += popcount(words[k]!);
		}


		if (size != 6) throw new Error("oops")
	})

	await import('typedfastbitset').then(({ TypedFastBitSet }) => {
		const b = new TypedFastBitSet(vals);
		bench("TypedFastBitSet.js	", () => {
			if (b.size() != 6) throw new Error("oops")
		})
	}).catch(() => {
		console.log("typedfastbitset not installed, skipping...")
		console.log("(use `pnpm install typedfastbitset` to add it to benchmarks)	")
	})

	// TODO CRoaring memoizes the cardinality, so this is not a fair comparison
	// await import('roaring').then(({ RoaringBitmap32 }) => {
	// 	const b = new RoaringBitmap32(vals);
	// 	bench("roaring", () => {
	// 		if (b.size != 6) throw new Error("oops")
	// 	})
	// }).catch(() => {
	// 	console.log("roaring not installed, skipping...")
	// 	console.log("(use `pnpm install roaring` to add it to benchmarks)	")
	// })
});

// })


suite("popcount", () => {

	// const uint128max = 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFFn;
	const uint64max = 0xFFFF_FFFF_FFFF_FFFFn;
	(() => {
		expect(popcount_bithacks_32n(0n)).toEqual(0)
		expect(popcount_bithacks_32n(0xffn)).toEqual(8)
		expect(popcount_bithacks_32n(0xff1n)).toEqual(9)
		expect(popcount_bithacks_32n(uint64max >> 32n)).toEqual(32)
		expect(popcount_bithacks_32n(uint64max)).toEqual(32)

		for (const fn of [popcount, popcount_simple, popcount_bithacks_64, /*TODO popcount_bithacks_128*/]) {
			expect(fn(0n), fn.name).toEqual(0)
			expect(fn(0xff08n), fn.name).toEqual(9)
			expect(fn(0xff08n << 48n), fn.name).toEqual(9)
			expect(fn(uint64max), fn.name).toEqual(64)
		}
	})()

	bench('baseline', () => {
		popcount(0n)
		popcount(uint64max)
	})

	bench('simple', () => {
		popcount_simple(0n)
		popcount_simple(uint64max)
	})

	bench('wasm_32', () => {
		popcount_wasm_32(0)
		popcount_wasm_32(0xffff_ffff)
	})
	bench('wasm_64', () => {
		popcount_wasm_64(0n)
		popcount_wasm_64(uint64max)
	})

	bench('bithacks_32', () => {
		popcount_bithacks_32(0)
		popcount_bithacks_32(0xffff_ffff)
	})
	bench('bithacks_32n', () => {
		popcount_bithacks_32n(0n)
		popcount_bithacks_32n(uint64max)
	})

	bench('bithacks_64', () => {
		popcount_bithacks_64(0n)
		popcount_bithacks_64(uint64max)
	})

	// bench('bithacks_128', () => {
	// 	popcount_bithacks_128(0n)
	// 	popcount_bithacks_128(uint128max)
	// })

	bench('wasm_v128', () => {
		popcount_wasm_v128(0n, 0n)
		popcount_wasm_v128(uint64max, uint64max)
	})
});

suite('popcount 32x4 (128 bit)', () => {
	bench('baseline', () => {
		for (let i = 0; i < 1_000; i++) {
			popcount_wasm_32(0)
			popcount_wasm_32(0)
			popcount_wasm_32(0)
			popcount_wasm_32(0)
		}
	})

	bench('bithacks x4', () => {
		for (let i = 0; i < 1_000; i++) {
			popcount_bithacks_x4_32(0, 0, 0, 0)
		}
	})

	bench('wasm_v128', () => {
		for (let i = 0; i < 1_000; i++) {
			popcount_wasm_v128(0n, 0n)
		}
	})
	bench('wasm_2xi64', () => {
		for (let i = 0; i < 1_000; i++) {
			popcount_wasm_2xi64(0n, 0n)
		}
	})



})

// works up to 32-bit-wide `v`s
function popcount_bithacks_32(v: number) {
	// via https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	// TODO: why not this?
	// v = v - ((v >> 1n) & 0x55555555n);
	// v = (v & 0x33333333n) + ((v >> 2n) & 0x33333333n);
	// return Number(((v + ((v >> 4n) & 0xF0F0F0Fn)) * 0x1010101n) >> 24n);

	let c = v - ((v >> 1) & 0x55555555);
	c = ((c >> 2) & 0x33333333) + (c & 0x33333333);
	c = ((c >> 4) + c) & 0x0F0F0F0F;
	c = ((c >> 8) + c) & 0x00FF00FF;
	c = ((c >> 16) + c) & 0x0000FFFF;
	return c;
}

function popcount_bithacks_x4_32(v1: number, v2: number, v3: number, v4: number) {

	let c1 = v1 - ((v1 >> 1) & 0x55555555);
	let c2 = v2 - ((v2 >> 1) & 0x55555555);
	let c3 = v3 - ((v3 >> 1) & 0x55555555);
	let c4 = v4 - ((v4 >> 1) & 0x55555555);
	c1 = ((c1 >> 2) & 0x33333333) + (c1 & 0x33333333);
	c2 = ((c2 >> 2) & 0x33333333) + (c2 & 0x33333333);
	c3 = ((c3 >> 2) & 0x33333333) + (c3 & 0x33333333);
	c4 = ((c4 >> 2) & 0x33333333) + (c4 & 0x33333333);
	c1 = ((c1 >> 4) + c1) & 0x0F0F0F0F;
	c2 = ((c2 >> 4) + c2) & 0x0F0F0F0F;
	c3 = ((c3 >> 4) + c3) & 0x0F0F0F0F;
	c4 = ((c4 >> 4) + c4) & 0x0F0F0F0F;
	c1 = ((c1 >> 8) + c1) & 0x00FF00FF;
	c2 = ((c2 >> 8) + c2) & 0x00FF00FF;
	c3 = ((c3 >> 8) + c3) & 0x00FF00FF;
	c4 = ((c4 >> 8) + c4) & 0x00FF00FF;
	c1 = ((c1 >> 16) + c1) & 0x0000FFFF;
	c2 = ((c2 >> 16) + c2) & 0x0000FFFF;
	c3 = ((c3 >> 16) + c3) & 0x0000FFFF;
	c4 = ((c4 >> 16) + c4) & 0x0000FFFF;
	return c1 + c2 + c3 + c4;
}

// works up to 32-bit-wide `v`s
function popcount_bithacks_32n(v: bigint) {
	// via https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	// TODO: why not this?
	// v = v - ((v >> 1n) & 0x55555555n);
	// v = (v & 0x33333333n) + ((v >> 2n) & 0x33333333n);
	// return Number(((v + ((v >> 4n) & 0xF0F0F0Fn)) * 0x1010101n) >> 24n);

	let c = v - ((v >> 1n) & 0x55555555n);
	c = ((c >> 2n) & 0x33333333n) + (c & 0x33333333n);
	c = ((c >> 4n) + c) & 0x0F0F0F0Fn;
	c = ((c >> 8n) + c) & 0x00FF00FFn;
	c = ((c >> 16n) + c) & 0x0000FFFFn;
	return Number(c);
}

// works up to 64-bit-wide `v`s
function popcount_bithacks_64(v: bigint) {
	// via https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	let c = v - ((v >> 1n) & 0x55555555_55555555n);
	c = ((c >> 2n) & 0x33333333_33333333n) + (c & 0x33333333_33333333n);
	c = ((c >> 4n) + c) & 0x0F0F0F0F_0F0F0F0Fn;
	c = ((c >> 8n) + c) & 0x00FF00FF_00FF00FFn;
	c = ((c >> 16n) + c) & 0x0000FFFF_0000FFFFn;
	c = ((c >> 32n) + c) & 0x00000000_FFFFFFFFn;
	return Number(c);
}

// works up to 64-bit-wide `v`s
const popcount_bithacks_64_busted = (() => {
	// via https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	const T0 = 0x5555_5555_5555_5555n // (T)~(T)0/3
	const T1 = 0x3333_3333_3333_3333n // (T)~(T)0/15*3
	const T2 = 0x1515_1515_1515_1515n // (T)~(T)0/255*15
	const T3 = 0x1010_1010_1010_1010n // (T)~(T)0/255

	return function popcount_bithacks_64(v: bigint) {
		v -= (v >> 1n) & T0;
		v = (v & T1) + ((v >> 2n) & T1);
		v = (v + (v >> 4n)) & T2;
		return Number((v * T3 >> (64n - 1n) * 8n));
	}
})();

// works up to 128-bit-wide `v`s
const popcount_bithacks_128 = (() => {
	// via https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	const T0 = 0x5555_5555_5555_5555_5555_5555_5555_5555n // (T)~(T)0/3
	const T1 = 0x3333_3333_3333_3333_3333_3333_3333_3333n // (T)~(T)0/15*3
	const T2 = 0x1515_1515_1515_1515_1515_1515_1515_1515n // (T)~(T)0/255*15
	const T3 = 0x1010_1010_1010_1010_1010_1010_1010_1010n // (T)~(T)0/255

	return function popcount_bithacks_64(v: bigint) {
		v -= (v >> 1n) & T0;
		v = (v & T1) + ((v >> 2n) & T1);
		v = (v + (v >> 4n)) & T2;
		return Number((v * T3 >> (128n - 1n) * 8n));
	}
})();

// // works up to 128-bit-wide `v`s
// const popcount_bithacks_128 = (() => {
// 	return function popcount_bithacks_128(v: bigint) {

// 	}
// })();

// function popcount_bithacks(v: bigint) {
// 	// via https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel ?
// 	// these don't work as-is, because `- 1n / 3n` gives us `0n`,.
// 	// rather than 0xFFF...FFF / 0x11 as desired
// 	v -= (v >> 1n) & (~0n / 3n);
// 	v = (v & (~0n / 15n * 3n)) + ((v >> 2n) & (~0n / 15n * 3n));
// 	v = (v + (v >> 4n)) & (~0n / 255n * 15n);
// 	return Number((v * ((~0n / 255n))) >> (64n - 1n) * 8n);
// }

// NB: only works with positive values, because a negative bigint has logically infinite popcount
function popcount_simple(v: bigint) {
	let count = 0;
	for (; v > 0; v >>= 1n) {
		count += Number(v & 1n)
	}
	return count
}


`
(module
	(func (export "i64.popcnt") (param i64) (result i32)
    local.get 0
    i64.popcnt
    i32.wrap_i64)
	(func (export "i32.popcnt") (param i32) (result i32)
    local.get 0
    i32.popcnt))
`
// via https://webassembly.github.io/wabt/demo/wat2wasm/
const wasm = (await
	WebAssembly.instantiateStreaming(
		fetch("data:application/wasm;base64,AGFzbQEAAAABCwJgAX4Bf2ABfwF/AwMCAAEHGwIKaTY0LnBvcGNudAAACmkzMi5wb3BjbnQAAQoOAgYAIAB7pwsFACAAaQsADARuYW1lAgUCAAABAA=="),
		{}));

const instance = wasm.instance;
export const popcount_wasm_64 = instance.exports['i64.popcnt'] as (v: bigint) => number;
export const popcount_wasm_32 = instance.exports['i32.popcnt'] as (v: number) => number;




`
(module
	(func (export "v128.popcnt") (param i64 i64) (result i32)
      (local $v v128)
	  ;; ## choice point: how do we populate a v128 from two i64s?
      ;; cf.  https://godbolt.org/z/GfzM9Y83d
      local.get 0
      i64x2.splat
      local.get 1
      i64x2.replace_lane 1
      ;;drop


      ;;v128.const i64x2 0x11 0xff
      ;;v128.const i64x2 0xf0ff_ffff_ffff_ffff 0x00ff_ffff_ffff_ffff
      ;; v128.const i8x16 0x11 0xff 0 0 0 0 0 0 0 0 0 0 0 0 0 0
      i8x16.popcnt
      i16x8.extadd_pairwise_i8x16_u
	  i32x4.extadd_pairwise_i16x8_u

      ;; i64x2.extadd_pairwise_i32x4_u
      ;;  ^ doesn't exist
      ;; ## choice point: how do we get the two halves back out and added together?
      local.tee $v

      v128.const i8x16 -1 -1 -1 -1 0 1 2 3 -1 -1 -1 -1 8 9 10 11
      i8x16.swizzle
      local.get $v
      i32x4.add
	  local.tee $v
      i32x4.extract_lane 1
      local.get $v
      i32x4.extract_lane 3
      i32.add

            ;;i64x2.extract_lane 0

      ;;i8x16.shuffle 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0

      ;; i64x2.extract_lane 0
      ;;local.get $v
      ;;i64x2.extract_lane 1
      ;;i64.add
      ;;i32.wrap_i64

      ;;drop
      ;;local.get $v
      ;;i32x4.extract_lane 3
        )
	(func (export "2xi64.popcnt")  ;; not to be confused with i64x2 (:
      (param i64 i64) (result i32)
      (local $v v128)
      local.get 0
      i64.popcnt
      local.get 1
      i64.popcnt

      i64.add
      i32.wrap_i64
        )
)	`;
// via https://webassembly.github.io/wabt/demo/wat2wasm/
// (tested with:
`
const wasmInstance =
      new WebAssembly.Instance(wasmModule, {});
//const popcnt = wasmInstance.exports['v128.popcnt'];
const popcnt = wasmInstance.exports['2xi64.popcnt'];


//const uint128max = 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFFn;
const uint64max = 0xFFFF_FFFF_FFFF_FFFFn;

console.log('0x' + popcnt(0x0n, 0x0n).toString(16));
console.log(popcnt(0x0n, 0x0n));
console.log(popcnt(0x1n, 0x1n));

console.log(popcnt(0xffn, 0x00n));
console.log(popcnt(uint64max, 0x0n));

console.log(popcnt(uint64max, uint64max));
console.log(popcnt(uint64max, uint64max -1n));
`;
// )

const wasm2 = (await
	WebAssembly.instantiateStreaming(
		fetch("data:application/wasm;base64,AGFzbQEAAAABBwFgAn5+AX8DAwIAAAceAgt2MTI4LnBvcGNudAAADDJ4aTY0LnBvcGNudAABCkgCOQEBeyAA/RIgAf0eAf1i/X39fyIC/Qz/////AAECA/////8ICQoL/Q4gAv2uASIC/RsBIAL9GwNqCwwBAXsgAHsgAXt8pwsAEgRuYW1lAgsCAAECAXYBAQIBdg=="),
		{}));

export const popcount_wasm_v128 = wasm2.instance.exports['v128.popcnt'] as (v0: bigint, v1: bigint) => number;
export const popcount_wasm_2xi64 = wasm2.instance.exports['2xi64.popcnt'] as (v0: bigint, v1: bigint) => number;


module none { }
module none { }
