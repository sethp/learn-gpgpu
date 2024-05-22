---
title: Experiment & Goals
description: ''
---

Four areas to experiment with communicating about:

1. SIMT
2. Logical/physical work mapping
3. Memory safety
4. Parallelism (/efficiency)


Given the `fill_idx` SPIR-V program loaded into the editor:


```
;;; ... required preamble

  ; FILL_IDX entry point
%main_fn = OpFunction %void_t None %void_fn_t
      %0 = OpLabel
      %2 = OpAccessChain %_ptr_Input_uint32_t %gl_GlobalInvocationID %n
      %3 = OpLoad %uint32_t %2
      %4 = OpAccessChain %_ptr_StorageBuffer_uint32_t %buf0 %3
           OpStore %4 %3
           OpReturn
           OpFunctionEnd
```

Goals:

- Notice that there's only "one" OpStore, but it gets executed multiple times
  -> How many times? (logical coords/sizing; work "size")
  -> How much parallelism? (physical coords)

- Notice that it's always storing the "same thing" (`%3`), but that thing is different depending on the "invocation ID"
  -> How does divergence work? (i.e. "on or off" execution masks, not the per-thread state storage in some late model chips)
  -> What happens with size mismatches? (automatic masking vs when manual branching is required; memory safety)

- Notice the impact of the kernel on "occupancy & residency"
  -> How many `fill_idx` kernels can "fit" at once? (occupancy)
  -> How does the GPU "pick" what to step? (residency)
  -> What does this mean about memory bandwidth? (parallelism/efficiency) [maybe ought to go to `vecadd` for this one, since that's got loads from a thread-dependant place]


# Suggested Exercises:

1. "run" and notice that the Same Instruction was executed Multiple Times
2. Change the `%n` (better name tbd) constant value from `0` to `1`, and notice that only one element gets written 16 times; now change the work size from `16 1 1` to `1 16 1` -> original behavior restored
3. "debug" and step through to the OpStore
  - print %3
  - "switch 2 0 0" and print %3 again (logical coords)
  - what's the largest thing you can "switch" to? (parallelism)
  - what happens when you de-select a lane? (divergence)
4. Change the work size and repeat #2
  - to values within small-ish bounds (mapping between work and sizing; memory safety)
  - to values bigger than the hardware (mapping between logical and physical coords; "scalable parallelism")
  - and/or: changing the hardware size


## Notes

*Note*: it still seems wild to me that accessing the thread/local/work id (there's so many names...) is an `OpLoad`; it's a "vector", but surely it's always mapped to a specialized hardware register and doesn't ever actually incur a memory access of any kind (right?)
  -> there are linear work ID models, too, that might be clearer to start with

*Q*: What communicates SIMT more clearly than presenting `Store %4 %3` and then dumping the resulting vector?


Idea: decorate the text at run-time with "what happened":

```
  OpStore %4 %3     ; %4 <ptr 0x1000> = { [0] = 0, [1] =  1,  .... }
```

Idea: redraw the `<textarea>` as having multiple "layers" that can be scrubbed through, where each "layer" is decorated like:


```
  OpStore %4 %3   ; %4 <ptr to elem 0 in arr at 0x1000> = %3 <0>
```

Idea: show "motion" of a vector slot by printing out the before/after of the array slot "around" the OpStore

This is how I would've (did?) learn this; by `printf` to map the symbolic reasoning to a concrete example

And/or: step debugger that steps over the OpStore, and a graphical visual representation "flashes" to indicate the writes

    [ 0 ] [ 1 ] [ 2 ] [ 3 ] [ 4 ] [ 5 ]


*Q*: How is it possible to identify which elements were written? How many times?

fixme: talvos ought to track uninitialized/never-written memory (and possibly written-but-not-read). Right now it's happily printing out whatever happened to be in the heap at that address, which makes it really hard to tell what slots were written to and which ones weren't.

Idea: present the final vector as some sort of heat map based on write frequency?

Idea: trace the output vector backwards to who wrote it ("step backwards?")
  something like:

    0. click "run"
    1. indicate a dump'd vector components (i.e. they're links, clicking on one)
    2. then, a list/table of "last written by tid { 1, 0, 0 } @ OpStore %4 %3 ; %4 <ptr 0x1004> = %3 <1> " shows up?
    3. Optionally: "step backwards" to see where %3 came from

*Q*: What communicates parallelism more clearly than "you can switch to another 'active thread' during a debugging session"?

fixme: Especially because currently talvos doesn't handle work that's concurrent-but-not-parallel (it refuses work that's larger than it hardware size).

Idea: Expand out the cores/lanes visualization & run at ~ 0.5 - 1 Hz?

*Note*: `DISPATCH` in tcf sets the global size; there is no way to set the local size (or offsets). Also, the linkage between `DISPATCH` and `ENTRY` and `OpEntryPoint` is _complex_; worse, it's talvos-specific. `OpExecutionMode` (not `..Model`!) is not less complex, but it's at least complex in a way that'll come up in other contexts.

fixme: talvos currently makes to attempt to model residency or occupancy, so the answer to "how many can fit" is the rather non-didactic (& unprincipled) "how many times can you click 'run' within the same millisecond to overlap the executions"

*Q*: How do we communicate why most kernels usually take a "data size N" param, and have an `if (idx < N)` guard? Currently, we get a lot of "memory access errors" when accessing beyond the end of the array, and the thread/block continue normally past the point of the error.

Idea: halt the program on a bad access

*Q*: "how efficient is this program?" / "does this change make it more or less efficient?"

  `if (idx % 2 == 0)` -> now your program runs at 1/2 speed
  something something "roofline graph"?
  and/or, a zactronics-style leaderboard histogram

### Bugs

1. dang renumbering; in the debugger the line shows up as `OpStore %17 %16`
2. stepping to exit w/ a disabled lane causes _hilarity_
3. Seth did an oopsie teaching Talvos about the notion of a "hardware size" so Talvos actually refuses work that's equal to its hardware size too

*Q*: when is it worth thinking about how to do testing for these labs?

Idea: Express the set of guided feedback (vs. "unguided") somewhere, so at least I (Seth) can manually validate it?

  guided:

    - changing the size to be bigger than the maximum limits (something like 2^32? and/or setting that limit waaay smaller than a real GPU?)
    - attempting to change the constant value to something outside of [0, 2]
    - resizing the buffer (knowing both when and how to do so)

  (mostly) un-guided:

    - modifications that cause the module to fail to parse
