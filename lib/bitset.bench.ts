import { bench, suite } from 'vitest';
import { popcount } from './bitset';

suite("popcount", () => {

	const uint64max = 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFFn;

	bench('baseline', () => {
		popcount(0n)
		popcount(uint64max)
	})

	bench('simple', () => {
		popcount_simple(0n)
		popcount_simple(uint64max)
	})
});

// function popcount_bithacks(v: bigint) {
// 	// via https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel ?
// 	// these don't work as-is, because `-1n / 3n` gives us `0n`,.
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
