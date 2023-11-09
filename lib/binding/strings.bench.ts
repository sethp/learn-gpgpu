import { bench, expect, suite } from 'vitest';
import { UTF8ArrayToString, lengthBytesUTF8, stringToUTF8Array } from './strings';

const onlyFast = true;

const corpus = [
	{ name: 'plain ascii', str: 'hello' },
	{ name: 'utf-16 surrogates', str: 'こんにちは' },
	{ name: 'medium, ascii', str: 'hello world'.repeat(2) },
	{ name: 'medium, mixed', str: 'hello こんにちは'.repeat(20) },
	...(onlyFast ? [] : [
		{ name: 'long, ascii-only', str: 'hello'.repeat(2000) },
		{ name: 'long, surrogates-only', str: 'こんにちは'.repeat(2000) },
	])
].map(o => Object.assign(o, {
	get utf16Len() {
		return o.str.length
	},
	get utf8Len() {
		return lengthBytesUTF8(o.str)
	}
}))

const format = "$name ($utf16Len UTF-16 code units, $utf8Len bytes)"

suite.each(corpus)(`length: ${format}`, ({ str }) => {
	bench('baseline', () => {
		lengthBytesUTF8(str)
	})

	bench('TextEncoder', () => {
		encoderLen(str)
	}, {
		setup: b => {
			b.opts.beforeAll = () => {
				expect(encoderLen(str)).toBe(lengthBytesUTF8(str))
			}
		}
	})

	var encoderLen = function () {
		var encoder = new TextEncoder();
		var buffer = new Uint8Array(1024);
		return function encoderLen(str: string) {
			let len = 0;
			while (str) {
				const { read, written } = encoder.encodeInto(str, buffer)
				len += written;
				str = str.slice(read)
			}
			return len
		}
	}()
})

suite.each(corpus)(`encoding: ${format}`, ({ str }) => {
	var buffer = new Uint8Array(lengthBytesUTF8(str) + 1);
	bench('baseline', () => {
		stringToUTF8Array(str, buffer, 0, buffer.length)
	});

	(() => {
		bench('TextEncoder', () => {
			doEncode(str)
		}, {
			setup: b => {
				b.opts.beforeAll = () => {
					const { read, written } = doEncode(str);
					expect(read).toBe(str.length)
					expect(written).toBe(lengthBytesUTF8(str))
				}
			},
			warmupIterations: 20,
		})

		let doEncode = function () {
			let encoder = new TextEncoder();
			return function doEncode(str: string) {
				return encoder.encodeInto(str, buffer)
			}
		}()
	})();
})

suite.each(corpus)(`decoding: ${format}`, ({ str }) => {
	var buffer = new Uint8Array(lengthBytesUTF8(str));
	{
		const { read, written } = new TextEncoder().encodeInto(str, buffer);
		expect(read).toBe(str.length)
		expect(written).toBe(lengthBytesUTF8(str))
	}

	bench('baseline', () => {
		UTF8ArrayToString(buffer, 0, buffer.length, buffer.length)
	})

	bench('TextDecoder', () => {
		doDecode(buffer)
	}, {
		setup: b => {
			b.opts.beforeAll = () => {
				const got = doDecode(buffer);
				expect(got).toStrictEqual(str);
			}
		}
	})

	var doDecode = function () {
		var decoder = new TextDecoder('utf-8');
		return function doDecode(buf: Uint8Array) {
			return decoder.decode(buf)
		}
	}()
})

suite.skipIf(onlyFast).skip.each([
	{ size: 32, label: "32 B" },
	{ size: 4 << 10, label: "4 KB" },
	{ size: 2 << 20, label: "2 MB" },
	// { size: 1 << 30, label: "1 GB" },
])('exploring different api shapes ($label)', ({ size }) => {
	var buf = new ArrayBuffer(size),
		bytes = new Uint8Array(buf),
		view = new DataView(buf);

	bench('baseline', () => {
		(function (...[_, bb, ...__]: Parameters<typeof stringToUTF8Array>) {
			bb[0]
		})("", bytes, 0, buf.byteLength)
	})


	bench('DataView', () => {
		(function stringToUTF8Array(_, v, ..._args) {
			v.getUint8(0)
		})("", view, 0, 0)
	})
	bench('new DataView', () => {
		(function stringToUTF8Array(_, v, ..._args) {
			v.getUint8(0)
		})("", new DataView(buf, 0, buf.byteLength))
	})
	bench('new Uint8Array', () => {
		(function stringToUTF8Array(_, bb, ..._args) {
			bb[0]
		})("", new Uint8Array(buf, 0, buf.byteLength))
	})
	// NB: NOT `.slice`, that copies the entire backing buffer!
	// which, besides being slow, also means the caller would
	// have to copy it _back_ afterwards
	bench('Uint8Array.prototype.subarray', () => {
		(function stringToUTF8Array(_, bb, ..._args) {
			bb[0]
		})("", bytes.subarray(0, buf.byteLength))
	})
})

// really slow, and/or doesn't work
suite.skip.each(corpus)(`streams ${format}`, ({ str }) => {
	var buffer = new Uint8Array(lengthBytesUTF8(str) + 1);

	(() => {
		bench('TextEncoderStream (simple)', async () => {
			await doEncode(str)
		}, {
			setup: b => {
				b.opts.beforeAll = async () => {
					const { read, written } = await doEncode(str);
					expect(read).toBe(str.length)
					expect(written).toBe(lengthBytesUTF8(str))

					var buffer2 = new Uint8Array(lengthBytesUTF8(str) + 1);
					const n = stringToUTF8Array(str, buffer2, 0, buffer2.length)
					expect(buffer.subarray(0, written)).toStrictEqual(buffer2.subarray(0, n))
				}
			}
		})

		let doEncode = function () {
			return async function doEncode(str: string) {
				let read, written = 0;
				await new ReadableStream({
					start(controller) {
						controller.enqueue(str)
						read = str.length;
						controller.close()
					}
				}).pipeThrough(new TextEncoderStream())
					.pipeTo(new WritableStream({
						write(chunk) {
							buffer.set(chunk, written)
							written += chunk.length;
						}
					}));
				return { read, written }
			}
		}()
	})();

	(() => {
		bench('TextEncoderStream (byob)', async () => {
			await doEncode(str)
		}, {
			setup: b => {
				b.opts.beforeAll = async () => {
					const { read, written } = await doEncode(str);
					expect(read).toBe(str.length)
					expect(written).toBe(lengthBytesUTF8(str))

					var buffer2 = new Uint8Array(lengthBytesUTF8(str) + 1);
					const n = stringToUTF8Array(str, buffer2, 0, buffer2.length)
					expect(buffer.subarray(0, written)).toStrictEqual(buffer2.subarray(0, n))
				}
			}
		})

		let doEncode = function () {
			return async function doEncode(str: string) {
				let encoder = new TextEncoderStream()

				encoder.writable.getWriter().write(str);
				const reader = encoder.readable.getReader({ mode: 'byob' });
				// ^ this explodes, because byob mode isn't supported
				// https://github.com/ricea/encoding-streams/blob/c21077c0016835b248e83b1cea96f16fa7b7a2e9/stream-explainer.md#non-goals
				let read = str.length, written = 0;
				let done = false;
				while (!done) {
					let value;
					({ done, value } = await reader.read(buffer.subarray(written)));
					written += value!.byteLength;
				}

				return { read, written }
			}
		}()
	})();
})


// well hello there
// https://streams.spec.whatwg.org/demos/streaming-element.html
// https://streams.spec.whatwg.org/demos/append-child.html
//
// hmm, ok, so things that are attractive about this:
//	- potentially can reduce copies with the "byob" bits
//  - async API for (safely?) moving bytes around and re-interpreting them "on the fly"
//  - maps nicely to the mental model of "this element is bound to this region of wasm memory"
//  - keeps me from having to write HTML into/via C++
//
// but:
//  - whoa that async overhead is costly! >500x slower than the sync version!
//		- maybe it's just microbenchmark bias?
//    - Except, a lot of the time I am just mapping small immediate strings, so async is pure overhead
//    - whole-system-composition wise, there's probably a tipping point around responsiveness?
//    - Especially when I really do have things that are being generated at percpetable frequencies
//  - Text{En,De}coderStream doesn't support `byob` mode? So they're really
//  - doesn't actually get us to zero copy; piercing the abstractions to let the browser's DOM just "use"
//    our WASM memory still may not be possible (we'd have to have e.g. ~static <option> elements somewhere
//    that each had had a pointer for their inner content string(s))
