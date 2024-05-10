import { describe, expect, test } from 'vitest';
import talvos from './talvos-wasm';
import main from './testdata/main.spvasm?raw'
// TODO: replace with main.spvasm ?
import fill from '../content/talvos/fill.spvasm.?raw'

describe('talvos', () => {
	test('inits', async () => {
		await talvos();
	})

	test('exception', async () => {
		let instance = await talvos();

		expect(() => instance.ccall('exception'))
			.toThrowErrorMatchingSnapshot();
	})
	test('assertion', async () => {
		let [stdout, stderr] = ['', ''];
		let instance = await talvos({
			print: (text: any) => { stdout += text; },
			printErr: (text: any) => { stderr += text; },
		});

		expect(() => instance.cwrap('assertion')())
			.toThrowErrorMatchingSnapshot();

		expect(stdout).toBe('');
		// TODO: this is sensitive to the line number of the assert
		expect(stderr).toMatchSnapshot();
	})
	test('oob_write', async () => {
		let [stdout, stderr] = ['', ''];
		let instance = await talvos({
			print: (text: any) => { stdout += text + "\n"; },
			printErr: (text: any) => { stderr += text + "\n"; },
		});


		instance.cwrap('test_entry', 'void', ['string', 'string', 'string'])(
			fill,
			'FILL',
			`
BUFFER a 64 UNINIT
DESCRIPTOR_SET 0 0 0 a

DISPATCH 17 1 1
			`
		)

		expect(stdout).to.be.empty;
		expect(stderr).toMatchInlineSnapshot(`
			"
			Invalid store of 4 bytes to address 0x1000000000040 (Device scope)
			    Entry point: %1 FILL
			    Invocation: Global(16,0,0) Local(0,0,0) Group(16,0,0)
			      OpStore %18 %14

			"
		`);
	})
	test('main kernel', async () => {
		let [stdout, stderr] = ['', ''];
		let instance = await talvos({
			print: (text: any) => { stdout += text + '\n'; },
			printErr: (text: any) => { stderr += text; console.error(text) },
		});

		instance.cwrap('test_entry', 'void', ['string', 'string', 'string'])(
			main,
			`main`,
			`
			BUFFER fill 64 UNINIT
			BUFFER series 64 UNINIT

			DESCRIPTOR_SET 0 0 0 fill
			DESCRIPTOR_SET 0 1 0 series

			#DISPATCH 16 1 1
			DISPATCH 1 1 1

			DUMP UINT32 fill
			DUMP UINT32 series
			`
		)

		expect(stderr).toEqual('');
		expect(stdout).toMatchInlineSnapshot(`
			"
			Buffer 'fill' (64 bytes):
			  fill[0] = 431188
			  fill[1] = 431188
			  fill[2] = 8
			  fill[3] = 8
			  fill[4] = 24
			  fill[5] = 26
			  fill[6] = 508392
			  fill[7] = 10
			  fill[8] = 10
			  fill[9] = 0
			  fill[10] = 48
			  fill[11] = 26
			  fill[12] = 512576
			  fill[13] = 8
			  fill[14] = 8
			  fill[15] = 0

			Buffer 'series' (64 bytes):
			  series[0] = 431124
			  series[1] = 73
			  series[2] = 512944
			  series[3] = 431164
			  series[4] = 33
			  series[5] = 1
			  series[6] = 144
			  series[7] = 49
			  series[8] = 512544
			  series[9] = 508608
			  series[10] = 6
			  series[11] = 6
			  series[12] = 24
			  series[13] = 26
			  series[14] = 0
			  series[15] = 6
			"
		`);
	})
})
