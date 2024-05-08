import { afterEach, beforeEach, describe, expect, test } from 'vitest';
import talvos, { type MainModule } from '../../wasm/talvos-wasm';
import { getEntry } from 'astro:content';

type GetEntryParams = Parameters<typeof getEntry>;
type Collection = GetEntryParams[0];
type Id = GetEntryParams[1];

async function getContents(c: Collection, f: Id) {
	const file = await getEntry(c, f);
	// there's a type-shaped puzzle box here to remove the `!`;
	// right now, `getContents` accepts anything that might be a "raw file",
	// and/or a collection/id pair that might not even exist.
	//
	// There Exists A Way to restrict the type signature to only allow raw file
	// collections / existing IDs within those collections, but I don't know the
	// typescript meta-language well enough to identify an effective approach.
	return file!.data.contents;
}

describe('fill', async () => {
	let [stdout, stderr] = ['', ''];

	beforeEach(() => {
		stdout = '';
		stderr = '';
	})

	const [
		validate_wasm,
		test_entry,
	] = await (function (p: Promise<MainModule>) {
		// we decorate with a tuple, because TS infers a simple array (i.e. T[])
		// which means everything is `T | undefined`
		return p.then((instance): [(...args: any[]) => any, (...args: any[]) => any] => [
			instance.cwrap('validate_wasm', 'boolean', ['string']),
			instance.cwrap('test_entry', 'void', ['string', 'string', 'string']),
		]);
	}(talvos({
		print: (text: any) => { stdout += text + '\n'; /* console.log(text) */ },
		printErr: (text: any) => { stderr += text + '\n'; console.error(text) },
	})));

	test('validates', async () => {
		expect(validate_wasm(
			await getContents('talvos', 'fill.spvasm'),
		)).to.equal(true, `spir-v failed validation`)
	})

	test('runs', async () => {
		let module = await getContents('talvos', 'fill.spvasm');
		let tcf = await getContents('talvos', 'fill.tcf');
		test_entry(
			module,
			'FILL',
			tcf,
		)

		expect(stderr).to.be.empty;
		expect(stdout).toMatchInlineSnapshot(`
			"
			Buffer 'a' (64 bytes):
			  a[0] = 1
			  a[1] = 1
			  a[2] = 1
			  a[3] = 1
			  a[4] = 1
			  a[5] = 1
			  a[6] = 1
			  a[7] = 1
			  a[8] = 1
			  a[9] = 1
			  a[10] = 1
			  a[11] = 1
			  a[12] = 1
			  a[13] = 1
			  a[14] = 1
			  a[15] = 1
			"
		`)
	})

	// test('exception', async () => {
	// 	let instance = await talvos();

	// 	expect(() => instance.ccall('exception'))
	// 		.toThrowErrorMatchingSnapshot();
	// })
	// test('assertion', async () => {
	// 	let [stdout, stderr] = ['', ''];
	// 	let instance = await talvos({
	// 		print: (text: any) => { stdout += text; },
	// 		printErr: (text: any) => { stderr += text; },
	// 	});

	// 	expect(() => instance.cwrap('assertion')())
	// 		.toThrowErrorMatchingSnapshot();

	// 	expect(stdout).toBe('');
	// 	// TODO: this is sensitive to the line number of the assert
	// 	expect(stderr).toMatchSnapshot();
	// })

})
