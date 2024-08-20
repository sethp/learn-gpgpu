// TypeScript bindings for emscripten-generated code.  Automatically generated at compile time.
declare namespace RuntimeExports {
    /**
     * @param {string=} returnType
     * @param {Array=} argTypes
     * @param {Object=} opts
     */
    function cwrap(ident: any, returnType?: string, argTypes?: any[], opts?: any): (...args: any[]) => any;
    function getExceptionMessage(ex: any): any;
    function decrementExceptionRefcount(ex: any): void;
    let wasmExports: any;
    /**
     * @param {string|null=} returnType
     * @param {Array=} argTypes
     * @param {Arguments|Array=} args
     * @param {Object=} opts
     */
    function ccall(ident: any, returnType?: string, argTypes?: any[], args?: any, opts?: any): any;
    function stackAlloc(sz: any): any;
    function stackSave(): any;
    function stackRestore(val: any): any;
    function stringToUTF8OnStack(str: any): any;
    function writeArrayToMemory(array: any, buffer: any): void;
    function stringToUTF8Array(str: any, heap: any, outIdx: any, maxBytesToWrite: any): number;
    /**
     * Given a pointer 'idx' to a null-terminated UTF8-encoded string in the given
     * array that contains uint8 values, returns a copy of that string as a
     * Javascript String object.
     * heapOrArray is either a regular array, or a JavaScript typed array view.
     * @param {number} idx
     * @param {number=} maxBytesToRead
     * @return {string}
     */
    function UTF8ArrayToString(heapOrArray: any, idx: number, maxBytesToRead?: number): string;
    let wasmMemory: any;
    let HEAPF32: any;
    let HEAPF64: any;
    let HEAP_DATA_VIEW: any;
    let HEAP8: any;
    let HEAPU8: any;
    let HEAP16: any;
    let HEAPU16: any;
    let HEAP32: any;
    let HEAPU32: any;
    let HEAP64: any;
    let HEAPU64: any;
}
interface WasmModule {
  _assertion(): void;
  _Session__create__(_0: number, _1: number): number;
  _Session__destroy__(_0: number): void;
  _Session__params_ref(_0: number): number;
  _Session__module_ref(_0: number): number;
  _Session__device_ref(_0: number): number;
  _Session__cores_ref(_0: number): number;
  _Session_run(_0: number): void;
  _Session_dumpBuffers(_0: number): void;
  _Session_start(_0: number, _1: number): void;
  _Session_printContext(_0: number): void;
  _Session_getCurrentId(_0: number, _1: number): void;
  _Session_getCurrentInsn(_0: number): number;
  _Session_step(_0: number, _1: number, _2: number): number;
  _Session_tick(_0: number): number;
  _Session_continue(_0: number, _1: number): void;
  _Session_print(_0: number, _1: number, _2: number): void;
  _Session_switch(_0: number, _1: number, _2: number): void;
  _validate_wasm(_0: number): number;
  _test_entry(_0: number, _1: number, _2: number): void;
  _test_entry_no_tcf(_0: number): void;
  _run_wasm(_0: number, _1: number): number;
  _debug_wasm(_0: number, _1: number): number;
  _Session_fetch_shrubbery(_0: number, _1: number): void;
  _exception(): void;
  _free(_0: number): void;
  _malloc(_0: number): number;
}

export type MainModule = WasmModule & typeof RuntimeExports;
export default function MainModuleFactory (options?: unknown): Promise<MainModule>;
