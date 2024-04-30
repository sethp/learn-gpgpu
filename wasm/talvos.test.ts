import { describe, expect, test } from 'vitest';
import talvos from './talvos-wasm';

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

})
