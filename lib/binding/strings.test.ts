import { expect, test, describe } from 'vitest';
import { UTF8ArrayToString, lengthBytesUTF8, stringToUTF8Array } from './strings';

describe('encoding', () => {
	test('lengthBytesUTF8', () => {
		for (const tc of [
			{ str: 'hello world', want: 11, },
			{ str: 'こんにちは', want: 15, },
		]) {
			const got = lengthBytesUTF8(tc.str)
			expect(got).toBe(tc.want);
		}
	})

	test('stringToUTF8Array', () => {
		for (const { str, want } of [
			{ str: 'hi', want: new Uint8Array([0x68, 0x69, 0x0]), },
			{ str: 'この', want: new Uint8Array([227, 129, 147, 227, 129, 174, 0x0]), },
		]) {
			const buf = new Uint8Array(new ArrayBuffer(128));
			const written = stringToUTF8Array(str, buf, 0, buf.byteLength)
			const got = buf.subarray(0, written + 1);
			expect(got).toStrictEqual(want);
		}
	})
})

describe('decoding', () => {
	// const abcs = Array(26).fill(undefined).map((_, i) => i + 0x61);
	const hello = [0x68, 0x65, 0x6c, 0x6c, 0x6f];
	const world = [0x77, 0x6f, 0x72, 0x6c, 0x64];

	test('UTF8ArrayToString', () => {
		for (const { buf, want, msg } of [
			{ buf: new Uint8Array(hello), want: 'hello', },
			{ buf: new Uint8Array([227, 129, 147, 227, 129, 174]), want: 'この', },

			{ buf: new Uint8Array([0x68, 0x69, 0x0, ...hello]), want: 'hi', msg: 'only up to the first nul terminator' },
			{
				// long enough to trip the TextDecoder code path
				buf: new Uint8Array(new Array(3).fill([...hello, 0x20]).flat().concat(world)),
				want: 'hello hello hello world',
			},
		]) {
			const got = UTF8ArrayToString(buf, 0, buf.byteLength)
			expect(got, msg).toBe(want);
		}
	})
})
