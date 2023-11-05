import { describe, expect, test } from 'vitest';
import { Ptr } from './binding/ptr';
import { ExecutionUniverse, States } from './executionUniverse';

expect.extend({
	toMatchBits(got, want) {
		const actual = `0x${got.toString(16)}`
		const expected = `0x${want.toString(16)}`
		return {
			pass: got === want,
			message: () => `expected ${actual} to match ${expected} bit-wise`,
			actual, expected
		}
	}
});

describe('ExecutionUniverse', () => {
	const SIZE = ExecutionUniverse.SIZE;
	test('zero', () => {
		const eu = new ExecutionUniverse(new Ptr(new ArrayBuffer(SIZE)));

		expect(eu.Cores).toEqual(0);
		expect(eu.Lanes).toEqual(0);
		expect(eu.SteppedCores.size()).toEqual(0);
		expect(eu.SteppedLanes.size()).toEqual(0);

		expect([...eu.LaneStates]).toEqual([]);
	})

	test('one zero state', () => {
		const buf = new ArrayBuffer(SIZE);

		{
			let bb = new Uint8Array(buf);
			bb.set([1], ExecutionUniverse.OFFSETS.cores);
			bb.set([1], ExecutionUniverse.OFFSETS.lanes);
		}

		const eu = new ExecutionUniverse(new Ptr(buf));

		expect(eu.Cores).toEqual(1);
		expect(eu.Lanes).toEqual(1);
		expect(eu.SteppedCores.size()).toEqual(0);
		expect(eu.SteppedLanes.size()).toEqual(0);

		const got = eu.LaneStates;
		expect([...got]).toHaveLength(1);
		// TODO expect.soft (but vitest-vscode barfs on it...)
		// TODO expect({ ...got.get(0).PhyCoord }).toEqual({ Core: 0, Lane: 0 }) ?
		expect({ Core: got.get(0).PhyCoord.Core, Lane: got.get(0).PhyCoord.Lane }).toEqual({ Core: 0, Lane: 0 })
		expect([got.get(0).LogCoord.X, got.get(0).LogCoord.Y, got.get(0).LogCoord.Z]).toEqual([0, 0, 0])
		expect(got.get(0).State).toEqual(States.Active)
	})

	test('non-zero states', () => {
		const buf = new ArrayBuffer(SIZE);

		{
			let bb = new Uint8Array(buf);
			bb.set([1], ExecutionUniverse.OFFSETS.cores);
			bb.set([2], ExecutionUniverse.OFFSETS.lanes);
		}

		{
			let bb = new Uint32Array(buf);
			bb.set([
				0x0101,                 // laneStates[0].phyCoord
				...Array(3).fill(0x01), // laneStates[0].logCoord
				0x01,                   // laneStates[0].state
				0x0202,                 // laneStates[1].phyCoord
				...Array(3).fill(0x03), // laneStates[1].logCoord
				0x02,										// laneStates[1].state
			], 24 >> 2);
			// bb.set([0x0101, ...Array(40).fill(0x01), 0x0102], 24 >> 2);
		}

		const eu = new ExecutionUniverse(new Ptr(buf));

		expect(eu.Cores).toEqual(1);
		expect(eu.Lanes).toEqual(2);
		expect(eu.SteppedCores.size()).toEqual(0);
		expect(eu.SteppedLanes.size()).toEqual(0);

		const got = eu.LaneStates;
		{
			let elems;
			expect(() => (elems = [...got])).not.toThrow();
			expect(elems).toHaveLength(2);
		}
		expect({ Core: got.get(0).PhyCoord.Core, Lane: got.get(0).PhyCoord.Lane }).toEqual({ Core: 1, Lane: 1 })
		expect({ Core: got.get(1).PhyCoord.Core, Lane: got.get(1).PhyCoord.Lane }).toEqual({ Core: 2, Lane: 2 })

		expect(got.get(0).State).toEqual(States.Inactive)
		expect(got.get(1).State).toEqual(States.AtBarrier)

		expect([got.get(0).LogCoord.X, got.get(0).LogCoord.Y, got.get(0).LogCoord.Z]).toEqual([1, 1, 1])
		expect([got.get(1).LogCoord.X, got.get(1).LogCoord.Y, got.get(1).LogCoord.Z]).toEqual([3, 3, 3])
	})

})
