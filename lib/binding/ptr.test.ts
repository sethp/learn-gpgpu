import { describe, expect, test } from 'vitest';
import { Ptr } from "./ptr";

describe("Ptr", () => {
	// [0, 1, 2, ...]
	const nums = new Uint8Array(...[Array(5).keys()]);

	test("slice", () => {
		const ptr = new Ptr(nums.buffer);

		for (const args of [
			[],
			[0], [1], [nums.length + 1],
			[0, 3], [0, 0], [0, nums.length], [0, nums.length + 1],
			[-1], [-3, -1], [-3, nums.length],
			[1, 0], [nums.length + 3, nums.length - 2]
		]) {
			let msg = `ptr.slice(...[${args}])`
			let want;
			expect(() => (want = [...nums.slice(...args)]),
				"invalid test case"
			).not.toThrow();

			msg += `; wanted [${want}]`
			let sliced;
			expect(() => (sliced = ptr.slice(...args)), msg).not.toThrow();
			sliced = sliced!;
			const got = new Uint8Array(sliced.data.buffer, sliced.addr, sliced.byteLength)
			expect([...got], msg).toEqual(want)
		}
	})
})
