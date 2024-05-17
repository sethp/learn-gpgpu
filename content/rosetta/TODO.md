no idea how to get this working, but rough idea:

(oh mah is the terminology a mess; everyone's using distinct terms to wrestle with the same very slippery concepts)

CUDA thread <-> OpenCL [??] <-> SPIR-V [??] <~> talvos Invocation (?)

CUDA warp ~> ??

CUDA SM -> "streaming multiprocessor"

CUDA software coordinates: {  [cluster], block @ { x y z }, thread @ { x y z } }  <--  "grid" fits somewhere in there
CUDA hardware coordinates: { grid_id, device, SM, warp, lane } (finest granularity `focus`: warp)

Talvos DSM -> descriptor set map
Talvos software coordinates: DispatchCommand {
  PipelineContext (AKA PipelineStage + DSM),
  BaseGroup @ { x y z },
  NumGroups @ { x y z },
}

Talvos "hardware" coordinates: PipelineExecutor { [OS thread,], Workgroup @ { x y z }, Invocation @ { x y z } }

Compute-specific terminology: "work item", "kernel" (do these come to us from OpenCL?)

SPIR-V makes no such distinction; its coordinate system is (via https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#_terms):

{
  Invocation Group
  [Subgroup,]
  Invocation
}


NB:
  Workgroup is ~ CUDA software "block"

  EntryPoint (when invoked) is ~ CUDA's "grid" ?

What do we call the atomic unit of parallelism?

  https://en.wikipedia.org/wiki/Flynn%27s_taxonomy ? no, doesn't really cover it




  https://en.wikipedia.org/wiki/Single_instruction,_multiple_threads has a nice table


| Nvidia CUDA |   OpenCL  |      Hennessy & Patterson[7]      |
|:-----------:|:---------:|:---------------------------------:|
|    Thread   | Work-item | Sequence of SIMD Lane operations  |
|     Warp    | Wavefront |    Thread of SIMD Instructions    |
|    Block    | Workgroup |      Body of vectorized loop      |
|     Grid    |  NDRange  |          Vectorized loop          |

[7]: John L. Hennessy; David A. Patterson (1990). Computer Architecture: A Quantitative Approach (6 ed.). Morgan Kaufmann. pp. 314 ff. ISBN 9781558600690.

  though aktshually I couldn't find it in there at all; I was looking in the 2nd edition and it was laid out differently (and never mentioned "vectorized"); the 6th edition is from 2019 which is very much not 1990 but has ISBN 9780128119068 (for the ebook)

Oh look, prior art: https://rocm.docs.amd.com/projects/HIP/en/latest/reference/terms.html#table-comparing-syntax-for-different-compute-apis
