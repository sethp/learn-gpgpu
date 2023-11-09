import { describe, expect, test } from 'vitest';
import { std$$string } from './stlBinding';
import { Ptr } from './binding/ptr';

describe('std::string', () => {
	const SIZE = std$$string.SIZE;
	test('zero', () => {
		const str = new std$$string(new Ptr(new ArrayBuffer(SIZE)));

		expect(str.length).toBe(0);
		expect(str.asString()).toStrictEqual("");
	})
	test('small string optimization ("__short")', () => {
		const buf = new ArrayBuffer(SIZE);

		{
			let bb = new Uint8Array(buf);
			bb.set([
				...[..."hello worl\0"].map((ch) => ch.charCodeAt(0)),
				10 /* length */
			]);
		}

		const str = new std$$string(new Ptr(buf));

		expect(str.__is_long, "the string ought to be inline optimized (SSO)").toBe(false);
		expect(str.length).toBe(10);
		expect(str.capacity).toBe(10);
		expect(str.asString()).toStrictEqual("hello worl");
	})
	test('heap-allocated string ("__long")', () => {
		const buf = new ArrayBuffer(SIZE * 2);

		{
			let bb = new Uint32Array(buf);
			bb.set([
				SIZE, // pointer __data;
				11, // size_type __size;
				11 | 0x8000_0000, // size_type __cap_: 33, __is_long_: 1;
			])
		}

		{
			let bb = new Uint8Array(buf, SIZE);
			bb.set([
				...[..."hello world\0"].map((ch) => ch.charCodeAt(0)),
			]);
		}

		const str = new std$$string(new Ptr(buf, 0, SIZE));

		expect(str.__is_long, "the string ought to be stored outboard (i.e. on the heap)").toBe(true);
		expect(str.capacity).toBe(11);
		expect(str.length).toBe(11);
		expect(str.asString()).toStrictEqual("hello world")
	})
})
