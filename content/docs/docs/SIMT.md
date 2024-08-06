---
title: SIMT
description: 'Experiments in explaining SIMT'
---

Some exposition on our journey through the world of modeling Simultaneous Instruction, Multiple Thread computations.

Core Questions:

1. When does one operation _dispatch_ relative to the previous one?
2. When does an operation _complete_ relative to its dispatch?
3. How do we define our computation such that it _scales_, both with the size of our data and our hardware?

# View A: "Control Blocks"

<svg xmlns="http://www.w3.org/2000/svg" class="vis0" data-cores="4" data-lanes="8">
<style>
g.core :hover,
g.core :focus {
	filter: drop-shadow(0 0 3px rgb(0 0 0 / 0.4));
}
g.core .ctrl {
	fill: chocolate;
}
.lane.selected,
.ctrl:has(~ .lane.selected):not(:has(~ .lane:not(.selected))) {
	filter: url(#inset-shadow);
}
.ctrl:has(~ .lane[data-state='not-launched']):not(:has(~ .lane[data-state='not-launched'])) {
	filter: opacity(60%);
}
.lane[data-state='active'] {
	fill: forestgreen;
}
.lane[data-state='inactive'] {
	fill: grey;
}
.lane[data-state='at-barrier'] {
	fill: lightseagreen;
}
.lane[data-state='at-breakpoint'] {
	fill: red;
}
.lane[data-state='at-assert'] {
	fill: orange;
}
.lane[data-state='at-exception'] {
	fill: darkred;
}
.lane[data-state='not-launched'] {
	fill: darkgray;
}
.lane[data-state='exited'] {
	fill: lightgray;
}
</style><defs> <filter id="inset-shadow"><feOffset dx="0" dy="0"></feOffset><feGaussianBlur stdDeviation="6" result="offset-blur"></feGaussianBlur><feComposite operator="out" in="SourceGraphic" in2="offset-blur" result="inverse"></feComposite><feFlood flood-color="black" flood-opacity=".95" result="color"></feFlood> <feComposite operator="in" in="color" in2="inverse" result="shadow"></feComposite><feComposite operator="over" in="shadow" in2="SourceGraphic"></feComposite></filter></defs><g class="core" transform="translate(0, 0)"> <rect class="ctrl" x="3.75" width="22.5" height="15"></rect> <rect x="32" width="15" height="15" data-phy-coords="{ 0, 0 }" class="lane selected" data-state="active" data-log-coords="{ 0, (0,0,0) }"></rect><rect x="48" width="15" height="15" data-phy-coords="{ 0, 1 }" class="lane selected" data-state="active" data-log-coords="{ 0, (1,0,0) }"></rect><rect x="64" width="15" height="15" data-phy-coords="{ 0, 2 }" class="lane selected" data-state="active" data-log-coords="{ 0, (2,0,0) }"></rect><rect x="80" width="15" height="15" data-phy-coords="{ 0, 3 }" class="lane selected" data-state="active" data-log-coords="{ 0, (3,0,0) }"></rect><rect x="96" width="15" height="15" data-phy-coords="{ 0, 4 }" class="lane selected" data-state="active" data-log-coords="{ 0, (4,0,0) }"></rect><rect x="112" width="15" height="15" data-phy-coords="{ 0, 5 }" class="lane" data-state="active" data-log-coords="{ 0, (5,0,0) }"></rect><rect x="128" width="15" height="15" data-phy-coords="{ 0, 6 }" class="lane" data-state="active" data-log-coords="{ 0, (6,0,0) }"></rect><rect x="144" width="15" height="15" data-phy-coords="{ 0, 7 }" class="lane" data-state="active" data-log-coords="{ 0, (7,0,0) }"></rect> </g><g class="core" transform="translate(0, 16)"> <rect class="ctrl" x="3.75" width="22.5" height="15"></rect> <rect x="32" width="15" height="15" data-phy-coords="{ 1, 0 }" class="lane" data-state="active" data-log-coords="{ 0, (8,0,0) }"></rect><rect x="48" width="15" height="15" data-phy-coords="{ 1, 1 }" class="lane" data-state="active" data-log-coords="{ 0, (9,0,0) }"></rect><rect x="64" width="15" height="15" data-phy-coords="{ 1, 2 }" class="lane" data-state="active" data-log-coords="{ 0, (10,0,0) }"></rect><rect x="80" width="15" height="15" data-phy-coords="{ 1, 3 }" class="lane selected" data-state="active" data-log-coords="{ 0, (11,0,0) }"></rect><rect x="96" width="15" height="15" data-phy-coords="{ 1, 4 }" class="lane selected" data-state="active" data-log-coords="{ 0, (12,0,0) }"></rect><rect x="112" width="15" height="15" data-phy-coords="{ 1, 5 }" class="lane selected" data-state="active" data-log-coords="{ 0, (13,0,0) }"></rect><rect x="128" width="15" height="15" data-phy-coords="{ 1, 6 }" class="lane selected" data-state="active" data-log-coords="{ 0, (14,0,0) }"></rect><rect x="144" width="15" height="15" data-phy-coords="{ 1, 7 }" class="lane selected" data-state="active" data-log-coords="{ 0, (15,0,0) }"></rect> </g><g class="core" transform="translate(0, 32)"> <rect class="ctrl" x="3.75" width="22.5" height="15"></rect> <rect x="32" width="15" height="15" data-phy-coords="{ 2, 0 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="48" width="15" height="15" data-phy-coords="{ 2, 1 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="64" width="15" height="15" data-phy-coords="{ 2, 2 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="80" width="15" height="15" data-phy-coords="{ 2, 3 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="96" width="15" height="15" data-phy-coords="{ 2, 4 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="112" width="15" height="15" data-phy-coords="{ 2, 5 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="128" width="15" height="15" data-phy-coords="{ 2, 6 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="144" width="15" height="15" data-phy-coords="{ 2, 7 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect> </g><g class="core" transform="translate(0, 48)"> <rect class="ctrl" x="3.75" width="22.5" height="15"></rect> <rect x="32" width="15" height="15" data-phy-coords="{ 3, 0 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="48" width="15" height="15" data-phy-coords="{ 3, 1 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="64" width="15" height="15" data-phy-coords="{ 3, 2 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="80" width="15" height="15" data-phy-coords="{ 3, 3 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="96" width="15" height="15" data-phy-coords="{ 3, 4 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="112" width="15" height="15" data-phy-coords="{ 3, 5 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="128" width="15" height="15" data-phy-coords="{ 3, 6 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect><rect x="144" width="15" height="15" data-phy-coords="{ 3, 7 }" class="lane" data-state="not-launched" data-log-coords="{ 0, (1,1,1) }"></rect> </g>
<text y="65"> <tspan x="0" dy="1em">physical {core, SIMT lane}:</tspan> <tspan class="physical">N/A</tspan> <tspan x="0" dy="1.2em">logical {workgroup, invocation}:</tspan> <tspan class="logical">N/A</tspan> </text> </svg>


<details><summary>(notes to the author)</summary>
damn, do we really need _all_ of the machinery? a (HiDPI) screenshot would almost work, but right now it's weird that the things highlight but the text doesn't change & they aren't clickable. on the other hand, it'd be easier to explain some of these things below if we _could_ just inline a snapshot'd execution of a particular program; plus, we'd avoid accidentally leaving out details (like: the text area looked like this at the time). hmm.

it also was a pain to get it "in" such that the parser wouldn't crash and burn, and I'm not at all convinced I've done a good job of it here still.
</details>

Roughly, this model displayed cores going down the page, lanes going right. Hovering over an element displayed its coordinates at the bottom, and—when the simulation was running—the

This let us talk about three things:

1. With appropriate coaching, stepping w/ some lanes deselected (as shown above) permits an experiment highlighting some of the dependent/independent relationship between Instructions and Threads.
    - By disabling some lanes and stepping the OpStore, the disabled part of the computation would be skipped. This demonstrated that each _operation_ was "on die" at most once, and the "multiple"-ness comes from exactly as many times as lanes of the core were active at that time.
    - Stepping with a whole core "disabled," on the other hand, allowed that same portion of the computation to resume when the core was re-enabled; demonstrating one of the kinds of independence enjoyed by cross-core operations not shared across lanes.
1. Work mapping, somewhat: by linking the hardware and logical coordinates, it was possible to "see" which parts of the program were executed by which hardware elements.
1. The "scalability" of SIMT models; since the program specifies parallelism in exactly one place ("OpExecutionGlobalSizeTALVOS"), we can "light up" more lanes and cores by changing just that one number.
    - Adding a control to expose the number of cores and lanes per core would have augmented the view with the capability to "reshape" the hardware to better show both the scalability and its interplay with the hardware scheduler.



# Notes

1. The split between "SIMT" and "Parallelism" is roughly "one core" vs "many cores"; it's not totally clear yet where "many programs" fit into the split, though.
1. Unfortunately, both "instruction" and "thread" are words that are so overloaded we prefer to avoid them. However, substitutes aren't in wide use, especially when it comes to describing GP-GPU computing, so we're sticking with "SIMT" for now.
    - The most useful possible alternative might be Single Program Multiple Data (SPMD, via Hwu et al. 2022), but that speaks more to the whole complex of cores. More to the point, it doesn't elucidate single-core execution.
    - While some late-model GPUs offer thread-level instruction granularity, that's a recent enough development that it's not yet planned here. Addressing it would require understanding its impacts on divergence and occupancy, which are both planned but not yet implemented in the model.
1. Whether or not introducing a model of pipelining is useful is similarly unclear. As few guarantees as GPUs provide, they do still have enough pipeline registers to conspire to make each "lane-slice" of work appear sequential, though, so from a debugging model standpoint we can treat them as such.
    - Doing so certainly more accurately characterizes the behavior of a single core—and very nicely motivates the masking-vs-branching dichotomy, especially as the pipeline length grows.
    - But, that also comes at a high cost for both the learner and the implementation. We'd need to break down all the SPIR-V opcodes (that we support, anyway) in a way that "makes sense" for whatever pipeline we pick.
1. In order to expose the difference between dispatch and completion, we need some sort of structural hazard that rate limits computtation. The memory controller is a natural first target for such a hazard, since moving bits to the compute units is very likely to be the limiting factor on any given GPU program (limited by arithmetic-clock-speed being the lucky few). So, we've largely grouped operations into "memory" and "not-memory," ignoring such things as floating-point or integer-addition pipelines (at least, for now).
    - Implementing a more accurate single-core pipeline model would unlock the capability to characterize different operations in terms of "µops"


# References

References:

- Hwu, Wen-Mei; Kirk, David. _Programming Massively Parallel Processors_, 4th ed, 2022. ISBN13: [9780323912310](https://openlibrary.org/books/OL36766483M/Programming_Massively_Parallel_Processors)
