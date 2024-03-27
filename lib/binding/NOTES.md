

## On inlining & composition of offsets (& `new`)

Reading (or writing) data consists of:

0. Knowing the length of the data
1. Finding the data in memory
2. Interpreting (or setting) those bytes according to some out-of-band schema

    For example, reading a 32-bit integer maps to:

    0. Length is 4 bytes (32 bits)
    1. Adding an offset (>= 0) to a pointer (like an array base pointer, or the current stack frame)
    2. Interpreting those bytes as a little-endian 2s-compliment integer

(doing this effectively requires precise agreement between all readers & writers)


(iterated) Nested field accesses look like:

0. Knowing the length of the final field, and what a pointer looks like on my architecture
1. While I still have steps left in the chain:
  1. Update my base pointer to the current field's location
  2. (sometimes) dereference my pointer (i.e. read the current pointee as a `usize` integer, providing a new pointer)
2. Interpret my final pointer's bytes as the final field

However, we ought to be able to collapse any nested accesses that don't dereference pointers (step 1.2) by simply summing their offsets; for example, given a contiguous array of structs of structs we might access something like `arr[idx].foo.bar`. We can proceed by:

0. Given (either by our own hand, or the compiler authors' decisions): `idx` is unknown until runtime, each array element is 42 bytes wide, .foo and .bar are at offsets 12 and 8 respectively, then
1. `arrOffset := idx * 42`
2. `ptr := arr + arrOffset`
3. `ptr += 12`
4. `ptr += 8`
5. `return  *(ptr+0)`, written this way because usually the ISA provides a load/store instruction that itself accepts a (~dozen bit fixed) offset.

Which takes approximately ~5 instructions that are all dependant on each other (i.e. contain data hazards) and share a structural hazard on the ALU, limiting the processor's ability to pipeline them. Alternatively, we could achieve the same result by:

0. (as above)
1. `ptr := arr + idx * 42` (assuming the ISA has a multiply-and-accumulate style instruction; otherwise this might need to be two steps)
2. `return *(ptr+20)` (pre-computing `20` as the sum `12+8`; x86 offers an addressing mode that combines this and the above into a single instruction)

Which is fewer instructions by at least 2 (and at most 4, depending on the ISA), but crucially has far fewer hazards: we're more clearly communicating to the µ-arch that only the load/store subsystem requires the output of the multiply, and that the `ptr` register is dead after passing its contents off to the memory banks.




#### Assumptions + Time

I'm (lightly) describing an ABI, aren't I? What problems come up?

Well, what about when there's a Good Reason to change the layout? How does

> (doing this effectively requires precise agreement between all readers & writers)

Work?

Because traditionally the agreement(s) are _implicit_ and hard to reverse; even if I "know" that I'm moving from field layout A to B (which, I don't, I'm a batch compiler that just saw field layout B and don't know anything about A), how do I take an existing `mov %eax, [%ebx*42 + 20]` instruction and precisely identify whether it 1) "belongs" to field layout A, and 2) modify it to now work with layout B? Transposing the problem is even harder: how do I know that I've successfully updated all references to layout A to match B? Without changing anything that's layout-A-like but actually "belonging" to layout B?

Sketches:

- use existing information When Possible, bailing when we don't have enough?
  - Like, if I know foo is moving from offset 12 to 16, simply Find All References to .foo and bump the reference by 4?
  - existing object code only; we might miss something tho (how can we be sure we found ALL the references? that's a halting problem, no?)
  - existing object code + source code history ?
  - DWARF debug info? Is this guaranteed to be complete? (not in the face of optimizations, right?)

- write down more information (o, hai wasm reference/interface/etc. types)


C++ shenanigans https://lobste.rs/s/bzcqjr/why_did_linux_choose_rust_not_c#c_isgrub
  > as long as you are working with std::bit_cast capable types you technically don’t need to worry about ABI issues at that point, beyond the relocation and COMDAT folding you’d already mentioned 🙂



### How do we get there?

Typically: combination of inlining and keyhole optimization (probably?)

  Aside: The current binding approach in javascript almost certainly doesn't even get close to this; each of those steps is a function call, so a more realistic pseudo-algorithm looks like:

  0. `var ptr; const ELEM_SIZE = 42; const FOO_OFFSET = 12; const BAR_OFFSET = 8;`
  1. `((idx) => ptr = arr + idx * ELEM_SIZE)(idx)`
  2. `(() => ptr += FOO_OFFSET)()`
  3. `(() => ptr += BAR_OFFSET)()`
  4. `return buffer.getUint32(ptr, littleEndian)` (which, hopefully v8 knows this is just `*(buffer + ptr)`?)

  Which means we've got function call overhead and mutliple object dereferences to contend with before we event get down to the ~5-instruction version.

  And even that's a little generous, because each step creates (via `new`) an instance that serves to
  bind various references + offsets together. Actually, two objects: one for the Ptr and one for the "wrapper"


#### How can we tell when we're there?

Comparing e.g.

1. Object.defineProperty vs.
2. class w/ getter function

or

1. `new` vs.
2. ..... (singletons somehow?)


We could use a microbenchmark? Higher ops/s would be better, but might not compose: could overstate the case for when we're doing the same access in a tight loop (vs. reality which is likely to be a mix of accesses across time?)

(cf. https://stackoverflow.com/questions/51607391/what-considerations-go-into-predicting-latency-for-operations-on-modern-supersca )



## On Arrays

Problems:

- Arrays are sparse
- Arrays are stringly-indexed
- Arrays are NOT "dereference transparent" (for the above two reasons; assuming v8 doesn't "see through" this?)
- Arrays are not implement-able in user code (only v8, or by proxies)
  - we get iterables, but there's no way to do `arr[i]` which e.g. d3's selection.join requires
    - should d3 change then? could use an iterable?


https://github.com/LastOliveGames/becsy/blob/4d8860cbc7114723a190486a27a6f544f4e2295a/src/type.ts#L195
  --> (statically[ish]) defines a property for each `i`, so `vec[0]` or `vec.x` are ~the same cost (though probably vec[idx] costs an itoa(idx), still. Is there a way to get out of _that_? surely v8 does something there already?)

https://github.com/bloomberg/record-tuple-polyfill/tree/master/packages/record-tuple-polyfill/src
  --> oh, also this is a really interesting source for impl ideas & further reading
  --> but can't do value classes: https://github.com/bloomberg/record-tuple-polyfill/blob/5f9cae34f0d331c4836efbc9cd618836c03e75f5/packages/record-tuple-polyfill/src/utils.js#L71-L75
    (why not? maybe some discussion over here: https://github.com/tc39/proposal-record-tuple/issues/358 )

https://github.com/tc39/proposal-structs
  --> ooh _hello_

https://github.com/WebAssembly/gc/blob/master/proposals/gc/Overview.md
  --> I really gotta get into this...

http://thecodebarbarian.com/thoughts-on-es6-proxies-performance
  -->  poor perf, probably prevents inlining?




Possible: hybrid approach for small arrays / arrays w/ known size(s), only paying for the proxy/`atoi` when we can't do that?
  (how expensive is it to define N nearly-identical properties? can we encheapen that by e.g. currying a function? )
  (also: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/freeze and
    and: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/seal  )


## On vectorizing javascript

Under what conditions does v8 know to vectorize a loop?

Loosely related:
  https://gamedev.stackexchange.com/questions/192502/what-data-structure-do-i-use-to-store-archetypes-in-ecs

also: https://github.com/SanderMertens/flecs
  --> AKA "do it in WASM"
  --> also: https://www.flecs.dev/explorer/



## On WasmGC MVP JS API

https://github.com/WebAssembly/gc/blob/master/proposals/gc/MVP-JS.md

> For now, require using the established pattern of exporting accessor functions to interact with Wasm data from JS.

  https://github.com/WebAssembly/gc/issues/279#issuecomment-1063238358

    We agreed at the subgroup meeting yesterday to split out a JS API for customizing accessors and prototypes for GC objects as a post-MVP proposal (https://github.com/WebAssembly/gc-js-customization), so for the MVP we'll be going with something that looks like the no-frills approach described here, at least for structs. We discussed a few options for how arrays might be accessed or created more directly from JS and we might choose to add some frills for arrays in the MVP if we can get a performance benefit from doing so.

    I'll close this issue for now, but feel free to reopen it if you have anything to add. Options for arrays should probably be discussed in fresh issues.


Related:

  https://github.com/Igalia/ref-cpp

  also: https://wingolog.org/archives/2023/10/19/requiem-for-a-stringref

