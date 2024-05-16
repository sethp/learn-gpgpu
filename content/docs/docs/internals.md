---
title: Internals
description: ''
---

# Descriptive

What talvos _is_: a straightforward mapping of SPIR-V opcodes to a multi-threaded work-stealing CPU model operating against a fully strongly consistent memory store.

This makes implementation of various common GPU (mis)uses kind of tricky. For example:

1. Debugging/executing; "step" ought to progress the entire "compute unit" (larger than a single "atom")
2. Physical limits; a `Device` has seemingly none
3. Implementing dispatch; "no guarantees" about children's work vs. parent (except, we do)
  - invoke "target" kernel -> desired happens-before w/ "setup" though (it "just works," but by accident)
  - "passing pointer to child" is verboten, but works fine in Talvos

Each of these has been addressed piecemeal as they've come up, but what this suggests is that our core model is a bit of a mis-match to the problem.

see also:

- async/concurrent execution: https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#asynchronous-concurrent-execution
- limits & sizes: https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#features-and-technical-specifications-technical-specifications-per-compute-capability

## Memory

- flat array
- no hierarchical fetches/coalescing
- ~strongly consistent (in single-threaded mode, anyway; otherwise tied to the platform's memory model)

# Normative

What talvos _ought to be_:

[ ] aware of / amenable to hardware "epochs" (like CDP1 vs CDP2; "independent thread scheduling")
[ ] explicitly modeling the hardware:
  [ ] scheduler
  [ ] register file w/ _occupancy_ and live-vs-executing tasks
  [ ] perf counters
  [ ] memory hierarchy
[ ] fully _concurrent_, but
[ ] pseudo-deterministically _parallel_

