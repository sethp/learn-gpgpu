import { afterEach, beforeEach, describe, expect, onTestFailed, test } from 'vitest';
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

describe('fill_idx', async () => {
	let [stdout, stderr] = ['', ''];

	beforeEach(() => {
		stdout = '';
		stderr = '';

		onTestFailed((_) => {
			if (stdout)
				console.log(stdout)
		})
	})

	const [
		validate_wasm,
		test_entry,
	] = await (function (p: Promise<MainModule>) {
		// we decorate with a tuple, because TS infers a simple array (i.e. T[])
		// which means everything is `T | undefined`
		return p.then((instance): [(...args: any[]) => any, (...args: any[]) => any] => [
			instance.cwrap('validate_wasm', 'boolean', ['string']),
			function () {
				const wasm_test_entry = instance.cwrap('test_entry_no_tcf', 'void', ['string'])
				return function (text: string) {
					return wasm_test_entry(text)
				}
			}()
		]);
	}(talvos({
		print: (text: any) => { stdout += text + '\n'; /* console.log(text) */ },
		printErr: (text: any) => { stderr += text + '\n'; console.error(text) },
	})));

	test('validates', async () => {
		expect(validate_wasm(
			await getContents('talvos', 'fill_idx.spvasm'),
		)).to.equal(true, `spir-v failed validation`)
	})

	test('runs', async () => {
		test_entry(
			await getContents('talvos', 'fill_idx.spvasm'),
		)

		expect(stderr).to.be.empty;
		expect(stdout).toMatchInlineSnapshot(`
			"
			Buffer 'a' (64 bytes):
			  a[0] = 0
			  a[1] = 1
			  a[2] = 2
			  a[3] = 3
			  a[4] = 4
			  a[5] = 5
			  a[6] = 6
			  a[7] = 7
			  a[8] = 8
			  a[9] = 9
			  a[10] = 10
			  a[11] = 11
			  a[12] = 12
			  a[13] = 13
			  a[14] = 14
			  a[15] = 15
			"
		`)
	})
})
