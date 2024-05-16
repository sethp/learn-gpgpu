---
title: Glossary
description: A collection of terms used in the project.
---

Afford/Affordance::

  Affordance is what the environment offers the individual that they can readily perceive.

  via https://en.wikipedia.org/wiki/Affordance


Concurrent::

   Composable independently executable work, i.e. work that is safe to re-order or overlap in execution without changing the outcome.

   See also: parallel; in our usage, concurrency permits simple parallelism, but does not require or even imply it.

   NB: this disagrees with CUDA's less precise terminology, where they often use "concurrent" and "concurrency" to mean "parallel" and "parallelism," e.g.

   > There is no guarantee of concurrent execution between any number of different thread blocks on a device.

  In our terminology, multiple "thread blocks" on a device _always_ express concurrency, because they represent units of work that _may_ be composed in any order and with any amount of overlap. On the other hand, "concurrent execution" here is a compound term best replaced by "parallelism"; there is no guarantee of paralellism, because there may be no available resources for the hardware scheduler to _make use_ of the concurrency, i.e. if every compute unit is busy, adding more concurrent tasks will not increase parallelism.

  **Question**: is there any use of "concurrent execution" that could not be replaced by "parallelism" in https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html ?

  Or, consider this other example:

   > Each device has its own default stream [...], so commands issued to the default stream of a device may execute out of order or concurrently with respect to commands issued to the default stream of any other device.

  We might prefer to say that the fact that different devices have distinct default streams implies work submitted to both queues will be treated _concurrently_ (i.e. as if it were safe to compose without regard to order or overlap) and therefore may run out of order or in parallel with each other.


Parallel::

  Simultaneous execution of work.

  NB: the work may be inter-dependent, i.e. it may not produce the same result (or perhaps even complete at all) if the work is re-ordered or the overlap changes. For example, a process A that launches B, sends a signal, and then waits for B to exit will _deadlock_ if B also waits for that signal and no parallelism exists.

  See also: concurrent; all concurrent work may be safely executed in parallel, but not all parallel work is concurrent: in fact, a good way to check for concurrency is to imagine adding or removing parallelism. If the computation always produces the same result, regardless of parallelism, then the work is more likely to be concurrent.

  Aside: Often problems arise when work that is _not_ concurrent is executed concurrently—that is, as if it were safe to execute in any order or with any degree of overlap. In those situations, so-called "concurrency bugs" arise because the work that was _thought_ to be concurrent turned out not to be correct under some degree of parallelism. For this reason, it's often easy to confuse "concurrent" and "parallel," because even though the misunderstanding was in whether or not the work was concurrent (i.e. was correct under all possible permutations), the mistake was exposed by "rubbing some parallelism on it."

