import { bench, suite } from 'vitest';

suite("hi", () => {

	bench('array creation', () => {
		new BigUint64Array(10000)
	})

	const buffer = new ArrayBuffer(10000 * BigUint64Array.BYTES_PER_ELEMENT)
	bench('array creation2', function () {
		// this.
		new BigUint64Array(buffer)
	})
})

suite("mom", () => {

	bench('array creation', () => {
		new BigUint64Array(10000)
	})

	bench('array creation2', () => {
		new BigUint64Array(20000)
	})
})
