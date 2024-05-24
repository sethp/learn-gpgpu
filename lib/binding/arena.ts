import type { Ptr } from "./ptr";

// //        offset v
// //   [==================] ->
// //   [===========|======] ->
// // [
// //   [==========],
// //              [|======],
// // ]
// function reslice(data: DataView, offset: size_t): [DataView, DataView] {
// 	return [
// 		new DataView(data.buffer, 0, data.byteOffset + offset),
// 		new DataView(data.buffer, this.addr + offset),
// 	]
// }

export class Arena {
	next: Ptr
	dtor: () => void

	constructor(start: Ptr, dtor?: () => void) {
		this.next = start;
		this.dtor = dtor ?? (() => { })
	}

	alloc(size: number) {
		if (size > this.size) {
			throw new Error(`OOM: Arena out of memory: requested ${size} bytes but only ${this.size} bytes remain`)
		}

		const [out, next] = this.next.reslice(size)
		this.next = next;

		return out
	}

	free(_ptr: Ptr) { }

	get size() {
		return this.next.byteLength
	}

	['__destroy__']() {
		this.dtor();
	}
}
