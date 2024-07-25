import { Ptr } from "./ptr"

export const LITTLE_ENDIAN = true; // wasm is always little endian

export interface HasAbi {
	SIZE: number
	new(ptr: Ptr): any
}


export function eagerDeref(T: HasAbi): HasAbi {
	class wrapper {
		static get SIZE() {
			return Ptr.SIZE;
		}
		constructor(ptr: Ptr) {
			return new T(ptr.deref(0, T.SIZE, LITTLE_ENDIAN));
		}
	}
	return wrapper;
}

export * from "./ptr"
export * from "./strings"
export * from "./arena"
