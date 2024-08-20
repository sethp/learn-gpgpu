import { describe, expect, test } from 'vitest';
import talvos from './talvos-wasm';
import main from './testdata/main.spvasm?raw'
// TODO: replace with main.spvasm ?
import fill from '../content/talvos/fill.spvasm.?raw'
import dup from './testdata/dup_entry.spvasm?raw'
import ticks from './testdata/ticks.spvasm?raw'
import { ExecutionUniverse, States } from '../lib/executionUniverse';
import { Arena, Ptr } from '../lib/binding';
import { BitSet } from '../lib/bitset';
import { Talvos$$Device, Talvos$$Dim3 } from '../lib/talvos';


describe('talvos', () => {
	test('inits', async () => {
		await talvos();
	})

	test('exception', async () => {
		let instance = await talvos();

		expect(() => instance.ccall('exception'))
			.toThrowErrorMatchingInlineSnapshot(`
				Exception {
				  "message": [
				    "std::runtime_error",
				    "hello: it's an exception!",
				  ],
				}
			`);
	})
	test('assertion', async () => {
		let [stdout, stderr] = ['', ''];
		let instance = await talvos({
			print: (text: any) => { stdout += text; },
			printErr: (text: any) => { stderr += text; },
		});

		expect(() => instance.cwrap('assertion')())
			.toThrowErrorMatchingInlineSnapshot(`[RuntimeError: unreachable]`);

		expect(stdout).toBe('');
		// TODO: this is sensitive to the line number of the assert
		expect(stderr).toMatchInlineSnapshot(`"Aborted(Assertion failed: false && "hello: it's an assertion!", at: ../wasm/talvos/tools/talvos-cmd/wasm.cpp,12,assertion)"`);
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

	test('ticks', async () => {
		let [stdout, stderr] = ['', ''];
		let instance = await talvos({
			print: (text: any) => { stdout += text + '\n' },
			printErr: (text: any) => { stderr += text; console.error(text) },
		});
		let { wasmExports: {
			// we use cwrap for Session__create__
			Session__destroy__,
			Session__device_ref,

			Session_start,
			// Session_step,
			Session_tick,

			Session_printContext,
			Session_getCurrentId,
			Session_switch,

		}, wasmMemory } = instance;

		// goal: get this thing "start"'d, assert something about its state, then step a tick, then do the same again
		const ptr = instance.cwrap('Session__create__', 'void', ['string', 'string'])(ticks, `
			ENTRY main
			EXEC`);

		// TODO: trying not to copy-pasta everything, but...
		function toCArgs(args: string[]) {
			const cstrs = args.map(instance.stringToUTF8OnStack) as number[];
			const argv = instance.stackAlloc(cstrs.length);
			cstrs.forEach((s, i) => {
				instance.HEAP32[(argv + i * 4) >> 2] = s;
			});

			return [cstrs.length, argv];
		}

		const arenaSize = 1 << 12;
		let arenaAlloc;
		try {
			arenaAlloc = instance._malloc(arenaSize);
			const arena = new Arena(new Ptr(wasmMemory.buffer, arenaAlloc, arenaSize));
			const lastOp = new ExecutionUniverse(arena.alloc(ExecutionUniverse.SIZE));
			// const laneMask = new BitSet(undefined, { data: arena.alloc(8).data });
			const Id: Talvos$$Dim3 = new Talvos$$Dim3(arena.alloc(Talvos$$Dim3.SIZE));

			Session_start(ptr, lastOp.ptr.asRef());
			const device = new Talvos$$Device(
				new Ptr(wasmMemory.buffer, Session__device_ref(ptr), Talvos$$Device.SIZE)
			);

			// TODO: once we fix the "start also steps once" bug
			// Session_step(ptr, laneMask.asRef(), lastOp.ptr.asRef());

			let [a, b, c, d, ...rest] = [...lastOp.LaneStates].map((e) => e.State);
			// We have four "active" lanes....
			expect([a, b, c, d]).toEqual([States.Active, States.Active, States.Active, States.Active]);
			// ... and only those four
			rest.forEach((e, i) =>
				expect(e, `mismatch starting at index ${i}: [${rest.slice(i)}]`).toBe(States.NotLaunched))

			Session_printContext(ptr);
			// NB: this is just the invocation @ { 3, 0, 0}
			Session_getCurrentId(ptr, Id.ptr.asRef());
			expect([...Id]).toEqual([3, 0, 0]);
			expect(stdout).toMatchInlineSnapshot(`
				"          OpLabel %1
				     %2 = OpAccessChain %11 %5 %12
				->   %3 = OpLoad %8 %2 %2 %4
				          OpReturn
				"
			`);
			stdout = '';

			Session_tick(ptr)

			// TODO: assert that we've got 1/4 of our slots fulfilled (?)

			// for now, just check that we're still "in" the first op...
			Session_getCurrentId(ptr, Id.ptr.asRef());
			Session_printContext(ptr);
			expect([...Id]).toEqual([0, 0, 0]);
			expect(stdout).toMatchInlineSnapshot(`
				"          OpLabel %1
				     %2 = OpAccessChain %11 %5 %12
				     %3 = OpLoad %8 %2 %2 %4
				->        OpReturn
				"
			`);
			stdout = '';

			Session_tick(ptr)
			Session_tick(ptr)
			Session_tick(ptr)

			// and now we're not
			Session_switch(ptr, ...toCArgs(['switch', '3']));
			expect(stderr).toMatchInlineSnapshot(`"Already executing this invocation!"`);
			Session_printContext(ptr);
			expect(stdout).toMatchInlineSnapshot(`
				"          OpLabel %1
				     %2 = OpAccessChain %11 %5 %12
				     %3 = OpLoad %8 %2 %2 %4
				->        OpReturn
				"
			`);
			stdout = '', stderr = '';
		} finally {
			Session__destroy__(ptr);
			if (arenaAlloc) instance._free(arenaAlloc);
		}
		expect(stderr).toEqual('');
		expect(stdout).toMatchInlineSnapshot(`
				""
		`);
	})
})
