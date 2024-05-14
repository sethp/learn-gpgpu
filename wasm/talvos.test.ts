import { describe, expect, test } from 'vitest';
import talvos from './talvos-wasm';
import main from './testdata/main.spvasm?raw'
// TODO: replace with main.spvasm ?
import fill from '../content/talvos/fill.spvasm.?raw'
import dup from './testdata/dup_entry.spvasm?raw'


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
			  fill[0] = 1
			  fill[1] = 1
			  fill[2] = 1
			  fill[3] = 1
			  fill[4] = 1
			  fill[5] = 1
			  fill[6] = 1
			  fill[7] = 1
			  fill[8] = 1
			  fill[9] = 1
			  fill[10] = 1
			  fill[11] = 1
			  fill[12] = 1
			  fill[13] = 1
			  fill[14] = 1
			  fill[15] = 1

			Buffer 'series' (64 bytes):
			  series[0] = 0
			  series[1] = 1
			  series[2] = 2
			  series[3] = 3
			  series[4] = 4
			  series[5] = 5
			  series[6] = 6
			  series[7] = 7
			  series[8] = 8
			  series[9] = 9
			  series[10] = 10
			  series[11] = 11
			  series[12] = 12
			  series[13] = 13
			  series[14] = 14
			  series[15] = 15
			"
		`);
	})
})
