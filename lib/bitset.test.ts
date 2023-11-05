import { describe, expect, test } from 'vitest';
import { BitSet, popcount } from './bitset';

expect.extend({
	toMatchBits(got, want) {
		const actual = `0x${got.toString(16)}`
		const expected = `0x${want.toString(16)}`
		return {
			pass: got === want,
			message: () => `expected ${actual} to match ${expected} bit-wise`,
			actual, expected
		}
	}
});

describe('BitSet', () => {
	test('basic', () => {
		const s = new BitSet(undefined, { data: new DataView(new ArrayBuffer(8)) })
		expect(s.add(1), "adding missing element").toBe(true);
		expect(s.add(1), "adding existing element").toBe(false);

		expect(s.has(1), "has existing element?").toBe(true);
		expect(s.has(42), "has missing element?").toBe(false);

		expect([...s]).toEqual([1]);
	})

	test('word boundaries', () => {
		const nn = [0, 63, 64, 127]
		const s = new BitSet(nn, { data: new DataView(new ArrayBuffer(16)) })

		for (const n of nn) {
			expect(s.add(n)).toBe(false)
		}

		s.words.forEach((word, i) => {
			// TODO https://stackoverflow.com/questions/67894491/use-jests-expect-extend-with-typescript-scoped-to-a-single-test-file ?
			// ah: https://vitest.dev/guide/extending-matchers.html
			// @ts-ignore
			expect(word, `at position \`${i}\``).toMatchBits((1n << 63n) | 1n)
		});

		expect([...s]).toEqual(nn);
	})


	test('clear', () => {
		const nn = [0, 1, 2, 127];
		const bs = new BitSet(nn, { data: new DataView(new ArrayBuffer(16)) })

		expect(bs.size()).toEqual(4)

		bs.clear()
		expect(bs.size()).toEqual(0)
	})
})

test('popcount', () => {
	expect(popcount(3n)).toBe(2)
	expect(popcount(4n)).toBe(1)
	expect(popcount(0xFFn)).toBe(8)
	expect(popcount(0xFFFFFFFF_FFFFFFFFn)).toBe(64)
	// this one's optional, really
	expect(popcount(0xFFFFFFFF_FFFFFFFF_1n)).toBe(65)
})
