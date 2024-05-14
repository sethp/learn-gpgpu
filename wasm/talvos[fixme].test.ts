import { describe, expect, test } from 'vitest';
import talvos from './talvos-wasm';
// TODO: replace ?
import fill from '../content/talvos/fill.spvasm.?raw'
import dup from './testdata/dup_entry.spvasm?raw'

// fixme: these tests are descriptive, not normative (they're bad feedback)
describe('talvos FIXMEs', () => {
	test('oob_write', async () => {
		let [stdout, stderr] = ['', ''];
		let instance = await talvos({
			print: (text: any) => { stdout += text + "\n"; },
			printErr: (text: any) => { stderr += text + "\n"; /*console.error(text)*/ },
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
	test('duplicate EntryPoint', async () => {
		let [stdout, stderr] = ['', ''];
		let instance = await talvos({
			print: (text: any) => { stdout += text + "\n"; },
			printErr: (text: any) => { stderr += text + "\n"; /*console.error(text)*/ },
		});


		instance.cwrap('test_entry', 'void', ['string', 'string', 'string'])(
			dup,
			'main',
			`DISPATCH 1 1 1`,
		)
		expect(stdout).to.be.empty;
		expect(stderr).toMatchInlineSnapshot(`
			"line 1: ERROR: Bad EntryPoint!
			"
		`)
	})
})
