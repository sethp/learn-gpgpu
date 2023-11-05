import type { Ptr } from "./binding/ptr";
import { BitSet } from "./bitset";


// the hardest part here seems to be keeping track of endianness, bases for
// offsets, and whether indexes are in bytes or in multi-byte units
//
// same as it ever was; this language has some weird defaults though
//   DataView is big endian by default, but all the TypedArrays are little-endian
//          (wasm is little-endian)
//   The TypedArrays offsets are in elem width chunks, but DataView.prototype.get* are all in bytes
//

// TODO move these into ./bind ?
const LITTLE_ENDIAN = true; // yay booleans
const UINT8 = { size: 1, get(ptr: Ptr, offset: number) { return ptr.data.getUint8(offset) } }
const UINT64 = { size: 8 }
function FIXED_ARRAY(elems: number) {
	return (elemT: { size: number }) => {
		return {
			size: elemT.size * elems
		}
	}
}
function RECORD(...elemTs: { size: number }[]) {
	return {
		size: elemTs.reduce((acc, eT) => acc + eT.size, 0)
	}
}

// these are "record" types that just so happens to match a C struct layout

export class LogCoord {
	// see talvos's wasm.cpp
	static get SIZE() { return 12; }

	constructor(public ptr: Ptr) {
		if (ptr.byteLength != LogCoord.SIZE)
			throw new Error(`bad size! got ${ptr.byteLength} wanted ${LogCoord.SIZE}`)
	}

	get X() { return this.ptr.data.getUint32(0, LITTLE_ENDIAN); }
	get Y() { return this.ptr.data.getUint32(4, LITTLE_ENDIAN); }
	get Z() { return this.ptr.data.getUint32(8, LITTLE_ENDIAN); }
};

export class PhyCoord {
	// see talvos's wasm.cpp
	static get SIZE() { return 2; }

	constructor(public ptr: Ptr) {
		if (ptr.byteLength != PhyCoord.SIZE)
			throw new Error(`bad size! got ${ptr.byteLength} wanted ${PhyCoord.SIZE}`)
	}

	get Core() { return this.ptr.data.getUint8(0); }
	get Lane() { return this.ptr.data.getUint8(1); }
};

export enum States {
	Active = 0,
	Inactive,
	AtBarrier,
	AtBreakpoint,
	AtAssert,
	AtException,
	NotLaunched,
	Exited,

	UNKNOWN,
}
// const MAX_STATE = Object.keys(States).reduce((last, k) => Math.max(last, States[k as any] as unknown as number), 0)

export class ExecutionUniverse {
	// see talvos's wasm.cpp
	static get SIZE() { return 1304; }

	static get OFFSETS() {
		return {
			cores: 0,
			lanes: 1,
			result: 2,

			steppedCores: 8,
			steppedLanes: 16,

			laneStates: 24,
		}
	};
	static get TYPES() {
		return {
			cores: UINT8,
			lanes: UINT8,

			steppedCores: UINT64,
			steppedLanes: UINT64,

			laneStates: FIXED_ARRAY(MAX_LANE_STATES)(RECORD(
				// PHY_COORD,
			))
		}
	};
	// TODO bind them together like so?
	// export const FIELDS = {
	// 	cores: {
	// 		offset: OFFSETS.cores,
	// 		type:
	// 	}
	// }

	constructor(public ptr: Ptr) {
		if (ptr.byteLength != ExecutionUniverse.SIZE)
			throw new Error(`bad size! got ${ptr.byteLength} wanted ${ExecutionUniverse.SIZE}`)
	}

	asRef() {
		return this.ptr.asRef();
	}

	get Cores() { return ExecutionUniverse.TYPES.cores.get(this.ptr, ExecutionUniverse.OFFSETS.cores); }
	get Lanes() { return ExecutionUniverse.TYPES.lanes.get(this.ptr, ExecutionUniverse.OFFSETS.lanes); }

	get SteppedCores() {
		return new BitSet(undefined, {
			data: this.ptr.slice(ExecutionUniverse.OFFSETS.steppedCores, ExecutionUniverse.OFFSETS.steppedCores + ExecutionUniverse.TYPES.steppedCores.size).data
		})
	}
	get SteppedLanes() {
		return new BitSet(undefined, {
			data: this.ptr.slice(ExecutionUniverse.OFFSETS.steppedLanes, ExecutionUniverse.OFFSETS.steppedLanes + ExecutionUniverse.TYPES.steppedLanes.size).data
		})
	}
	get LaneStates() {
		var i = 0;
		var n = Math.min(this.Cores * this.Lanes, MAX_LANE_STATES);
		const ptr = this.ptr.slice(24)
		// return new Proxy({
		const arr = {
			get(idx: number) {
				const elemPtr = ptr.slice(20 * idx, 20 * idx + 20)
				return {
					get PhyCoord() {
						return new PhyCoord(elemPtr.slice(0, 2))
					},
					get LogCoord() {
						return new LogCoord(elemPtr.slice(4, 16))
					},
					get State(): States {
						// const ret = elemPtr.data.getUint32(4 >> 2);
						const ret = elemPtr.data.getInt32(16, LITTLE_ENDIAN);
						// TODO throwing exceptions from getters causes Bad Times
						// if (ret >= States.MAX || ret < 0) throw new Error(`invalid state: ${ret}`)
						// if (ret >= States.UNKNOWN) return States.UNKNOWN;
						// if (ret < 0) return States.UNKNOWN;
						return ret
					},
				};
			},

			[Symbol.iterator]: () => ({
				next() {
					if (i >= n)
						return { done: true, value: undefined as never }

					return {
						done: false, value: arr.get(i++),
					}
				}
			})
		};
		return arr
		// // this (+ `return new Proxy({` above ) via https://stackoverflow.com/a/57634753/151464
		// // but: it's a bummer of an idea, because all the property accesses get stringified
		// , {
		// 	get(target, p) {
		// 		if (p in target) {
		// 			// @ts-ignore
		// 			return target[p]
		// 		}

		// 		throw new Error(`hi ${String(p)} ${typeof p}`)

		// 		if (typeof p == "number") {

		// 			// TODO no copy
		// 			return [...target][p]
		// 		}
		// 	},
		// })
	}
}

export const MAX_LANE_STATES = 64;
