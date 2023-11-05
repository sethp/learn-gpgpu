import { describe, expect, test } from 'vitest';
import { Arena } from "./arena";
import { Ptr } from "./ptr";

describe("Arena", () => {
	test(`alloc`, () => {
		const arena = new Arena(new Ptr(new ArrayBuffer(8)));
		for (const b of [1, 1, 2, 4]) {
			const msg = `alloc(${b}) from arena (${arena.size} bytes remaining)`;
			let got;
			expect(() => (got = arena.alloc(b)), msg).not.toThrow()
			expect(got!.byteLength, msg).toEqual(b);
		}

		expect(() => { arena.alloc(1) }).toThrow(/oom|memory/i);
	})
	test("free", () => {
		const arena = new Arena(new Ptr(new ArrayBuffer(8)));

		arena.free(arena.alloc(8));
	})
})
