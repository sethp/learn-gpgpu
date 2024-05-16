# NOTES

# snapshot testing

https://vitest.dev/guide/snapshot.html

Updating:
```
pnpm run vitest -u
```


# submodules

It's possible for local state to be out-of-sync with CI after committing to just one directory.

Workflow for threading changes across multiple places?

```
git -C wasm/talvos commit -av
git add wasm/talvos
```

Will `git push` push both? No, confirmed it does not.

Instead:
```
git -C wasm/talvos push
git push
```

# entr


Oh, neat: when I did it bad (while loop that never exits), `entr` gets re-parented to systemd when I kill the bash process:

```
/systemd(1)───systemd(2024)───entr(4001943)───build-wasm.sh(4091019)───build-wasm.sh(4091021)───pstree(4091022)
```

(via `pstree -ps $$` at the top of the script)


this is very fun! `entr` still holds on to the pty, so it will write things to the terminal even when other commands are running; that makes it seem *spooky*

also handy: `pstree -sp $(pgrep entr)`

# DWARF debugging w/ Chrome Devtools

Compilation Units have both a name, and a "compilation directory"

The way that the chrome devtools seems to look for sources is approximately:

1) For a relative path name with an absolute path comp dir: the file:// concatenation of the comp dir and name, e.g.

    ```
    Unit at <.debug_info+0x7e0caa>: system/lib/compiler-rt/__trap.c
        comp dir: /emsdk/emscripten
    ```

  -> `file:///emsdk/emscripten/system/lib/compiler-rt/__trap.c`

2) For an absolute path name: just the file:// of the name itself

    ```
    Unit at <.debug_info+0x59432e>: /usr/src/SPIRV-Tools/source/val/basic_block.cpp
        comp dir: /usr/src/SPIRV-Tools/build
    ```

    -> `file:///usr/src/SPIRV-Tools/source/val/basic_block.cpp`


3) For a relative path comp dir with a relative path, we get a HTTP request:

    ```
    DWO Unit ID 101ee0de7efe5452, name: ../wasm/talvos/tools/talvos-cmd/wasm.cpp
    Unit at <.debug_info+0x2c>: <no name>
        comp dir: .
    ```

    (from a http://localhost:4321/wasm/talvos-wasm.wasm & http://localhost:4321/wasm/talvos-wasm.dwp )
    -> `http://localhost:4321/wasm/talvos/tools/talvos-cmd/wasm.cpp`


Oh, or is the difference between 1-2 & 3 that the latter is in the `.dwp`, and the former are in the `.wasm`?
  -> could check that by just turning off `-gsplit-dwarf` entirely

Hmm, where are the headers? e.g. `CommandFile.h` ?

Debugging into an "immediate" dependency? Does it have to be compiled with `-ffile-prefix-map`?

Debugging into implicit system-level dependencies like `system/lib/compiler-rt/__trap.c` or `system/lib/compiler-rt/stack_ops.S` ?


## -gseparate-dwarf=...

With `-gseparate-dwarf=...`, Chrome always makes a (spurious?) request for .dwp.dwp ?

With `-gseparate-dwarf=...`:

    llvm-objdump --full-contents  -j external_debug_info ./wasm/talvos-wasm.wasm

    ./wasm/talvos-wasm.wasm:        file format wasm
    Contents of section external_debug_info:
    0000 0f74616c 766f732d 7761736d 2e647770  .talvos-wasm.dwp

besides the leading `0000 0f` that looks like the string I passed to `-gseparate-dwarf=...`, so that's nice?

Also, there's no debuginfo whatsoever in the `.wasm`

Also,
  "The debug information for ... is incomplete"

  Failed to load debug file "tools/talvos-cmd/CMakeFiles/talvos-wasm.dir/wasm.cpp.dwo".
  Failed to load debug file "/wasm/talvos-wasm.dwp.dwp"

(thankfully, those are both http requests, at least?)

removing `-gsplit-dwarf` does change the outcome there, but it'd be nice to have faster link times _and_ faster downloads.

(hmm, I wonder if I could `emdwp` the separate'd dwarf info? as a way to get the minimum size without needing to serve all the .dwo assets?)

## emdwp

With a separate `emdwp` invocation, there's no `external_debug_info` section, so the discovery has to be "magical", but we also don't get a spurious `.dwp.dwp` request

Also, the `.dwp` produced has no `.debug_info` section (unlike the `.dwp` produced by `-gseparate-dwarf=`!), which makes it ~not stand-alone

Also requires (/allows?) `-gsplit-dwarf` during compilation

Also, the standard `-gsplit-dwarf` skeleton debuginfo is in the `.wasm` (+ visibility, at the cost of size)

# vscode w/ talvos (clangd + cmake)

`ComputePipeline.cpp` can't find its header tho, and `.clangd` inside of talvos reaches "outside" the project to find the SPIRV stuff (that's a different item though).

Hmm, trying to generate compile_commands.json, running into trouble:

```
[cmake] -- SPIRV-Tools library: SPIRV_TOOLS_LIB-NOTFOUND
[cmake] CMake Error at lib/talvos/CMakeLists.txt:36 (message):
[cmake]   SPIRV-Tools library required, try setting SPIRV_TOOLS_LIBRARY_DIR.
```

Oh, I never got this working before either. Looks like somehow I accidentally my way into a compile_commands.json that worked, and never looked back?

```
(old talvos dir) $ find . -name compile_commands.json
./build/host/compile_commands.json
./build/emscripten/compile_commands.json
./build/compile_commands.json
```

Did I "just" install the library? Oh! I did, that `./build` is the host build, not the emscripten build.

```
$ cmake --install build/enscripten # sic
-- Install configuration: "RelWithDebInfo"
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/libSPIRV-Tools-opt.a
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-opt/SPIRV-Tools-optTargets.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-opt/SPIRV-Tools-optTargets-relwithdebinfo.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-opt/SPIRV-Tools-optConfig.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/libSPIRV-Tools-reduce.a
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-reduce/SPIRV-Tools-reduceTarget.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-reduce/SPIRV-Tools-reduceTarget-relwithdebinfo.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-reduce/SPIRV-Tools-reduceConfig.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/libSPIRV-Tools-link.a
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-link/SPIRV-Tools-linkTargets.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-link/SPIRV-Tools-linkTargets-relwithdebinfo.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-link/SPIRV-Tools-linkConfig.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/libSPIRV-Tools-lint.a
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-lint/SPIRV-Tools-lintTargets.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-lint/SPIRV-Tools-lintTargets-relwithdebinfo.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-lint/SPIRV-Tools-lintConfig.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/libSPIRV-Tools-diff.a
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-diff/SPIRV-Tools-diffTargets.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-diff/SPIRV-Tools-diffTargets-relwithdebinfo.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-diff/SPIRV-Tools-diffConfig.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/libSPIRV-Tools.a
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/libSPIRV-Tools-shared.a
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools/SPIRV-ToolsTarget.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools/SPIRV-ToolsTarget-relwithdebinfo.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools/SPIRV-ToolsConfig.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-lesspipe.sh
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-as.js
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-dis.js
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-val.js
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-opt.js
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-cfg.js
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-link.js
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-lint.js
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-objdump.js
-- Installing: /home/seth/.emscripten_cache/sysroot/bin/spirv-reduce.js
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-tools/SPIRV-Tools-toolsTargets.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-tools/SPIRV-Tools-toolsTargets-relwithdebinfo.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/cmake/SPIRV-Tools-tools/SPIRV-Tools-toolsConfig.cmake
-- Installing: /home/seth/.emscripten_cache/sysroot/include/spirv-tools/libspirv.h
-- Installing: /home/seth/.emscripten_cache/sysroot/include/spirv-tools/libspirv.hpp
-- Installing: /home/seth/.emscripten_cache/sysroot/include/spirv-tools/optimizer.hpp
-- Installing: /home/seth/.emscripten_cache/sysroot/include/spirv-tools/linker.hpp
-- Installing: /home/seth/.emscripten_cache/sysroot/include/spirv-tools/instrument.hpp
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/pkgconfig/SPIRV-Tools.pc
-- Installing: /home/seth/.emscripten_cache/sysroot/lib/pkgconfig/SPIRV-Tools-shared.pc
```

ah, and now I get a compile_commands.json and "all" is "well".

# cmake "superproject" build

Why do a single CMake "superproject"? The IDE tooling all wants a single `compile_commands.json`, and this seemed the most expedient way to glom them all together. The sticky part was getting the ad-hoc discovery mechanism(s) to all agree. The library part was especially strange, since it has to be a `SPIRV-Tools` magic string (that matches the target name) in "superproject" mode for CMake to understand the dependency link, but then `wasm-ld` wasn't able to find it in subproject mode. So, one more layer of indirection!

Talvos was doing discovery differently than everyone else, and I couldn't figure out how to get the `find_library` call to work at all with discovering a the SPIRV-Tools as a sibling target. Luckily, (ab)using `find_project` made for a CMakeLists.txt that worked for both talvos stand-alone (with system-installed headers/tools), and also as part of the combined cmake project.

Moving the build "up" a level was pretty straightforward after that:

```
   cp --verbose --update=older build/emscripten-docker/tools/talvos-cmd/talvos-wasm.* "$1"
-> cp --verbose --update=older build/emscripten-docker/talvos/tools/talvos-cmd/talvos-wasm.* "$1"
```

(as an aside: cmake's namespacing doesn't make a lot of sense to me. Every `add_subdirectory` gets reflected in the build output, but the `target` namespace is global, at least by default?)

## avoiding double-building SPIRV-Tools

Is it sensible to try and have a layer-per-dependency in the docker build? I'm not sure, but I had it before I moved over to submodules for the two dependencies.

This ought to be possible by preferring that `find_package` locates the already-built SPIRV-Tools instead of the sibling project. There seems to be a mode suited for this task:

  https://cmake.org/cmake/help/latest/variable/CMAKE_FIND_PACKAGE_PREFER_CONFIG.html

But it doesn't work on its own (it's still rebuilding SPIRV-Tools, even though libSPIRV-Tools.a is already in the sysroot). Maybe it's finding the project via https://cmake.org/cmake/help/latest/command/find_package.html#id9 ("Config Module Search Procedure") ?

Once `-USPIRV-Tools_DIR` was being passed to the configure step (thanks, cache vars!), then I tried:

```
find_package(SPIRV-Tools REQUIRED NO_MODULE
             NO_DEFAULT_PATH
             NO_PACKAGE_ROOT_PATH
             NO_CMAKE_PATH
             NO_CMAKE_ENVIRONMENT_PATH
             NO_SYSTEM_ENVIRONMENT_PATH
             NO_CMAKE_PACKAGE_REGISTRY
             NO_CMAKE_BUILDS_PATH
             NO_CMAKE_SYSTEM_PATH # vs. NO_CMAKE_INSTALL_PREFIX
             NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
             NO_CMAKE_FIND_ROOT_PATH
)

if (NOT SPIRV-Tools_DIR)
	message(FATAL_ERROR "no SPIRV-Tools found")
else()
	message(STATUS "SPIRV-Tools found ${SPIRV-Tools_DIR}")
endif()
```

which successfully failed (I've turned off all of the possible search locations). That let me bisect:

```
# find_package(SPIRV-Tools REQUIRED NO_MODULE)
# -> found /emsdk/upstream/emscripten/cache/sysroot/lib/cmake/SPIRV-Tools

# find_package(SPIRV-Tools REQUIRED NO_MODULE
#              NO_DEFAULT_PATH
#              NO_PACKAGE_ROOT_PATH
#              NO_CMAKE_PATH
#              NO_CMAKE_ENVIRONMENT_PATH
#              NO_SYSTEM_ENVIRONMENT_PATH
#              NO_CMAKE_PACKAGE_REGISTRY
#              NO_CMAKE_BUILDS_PATH
#              NO_CMAKE_SYSTEM_PATH # vs. NO_CMAKE_INSTALL_PREFIX
#              NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
#              NO_CMAKE_FIND_ROOT_PATH
# )
# -> not found (expected)

# find_package(SPIRV-Tools REQUIRED NO_MODULE
#              NO_CMAKE_PACKAGE_REGISTRY
#              NO_CMAKE_BUILDS_PATH
#              NO_CMAKE_SYSTEM_PATH # vs. NO_CMAKE_INSTALL_PREFIX
#              NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
#              NO_CMAKE_FIND_ROOT_PATH
# )
# -> not found (so, the "finder" ought to be in the other half)

find_package(SPIRV-Tools REQUIRED NO_MODULE
             NO_DEFAULT_PATH
             NO_PACKAGE_ROOT_PATH
             NO_CMAKE_PATH
             NO_CMAKE_ENVIRONMENT_PATH
             NO_SYSTEM_ENVIRONMENT_PATH
)
# -> not found (?!)
```

which immediately revealed there's something wacky going on in there (probably two of the options overlap, there's 10 of them and only 9 "steps" on the documentation page). But also, that neither mechanism is doing what I want because this is what they're ending up resolving to:

```
# cat /emsdk/upstream/emscripten/cache/sysroot/lib/cmake/SPIRV-Tools/SPIRV-ToolsConfig.cmake
include(${CMAKE_CURRENT_LIST_DIR}/SPIRV-ToolsTarget.cmake)
if(TARGET SPIRV-Tools)
    set(SPIRV-Tools_LIBRARIES SPIRV-Tools)
    get_target_property(SPIRV-Tools_INCLUDE_DIRS SPIRV-Tools INTERFACE_INCLUDE_DIRECTORIES)
endif()
```

`if(TARGET SPIRV-Tools)` it just goes ahead and says "yeah, link against the sibling project": the opposite of my goal. Cool.

I had been trying to avoid inventing a project-specific convention for "use find_library instead of find_project", oh well.

# Testing `fill.spvasm`

Getting very weird stderr output:

```
line 1: Unrecognized command 'emsc'
```

Trying to debug it, but "step into" doesn't work to cross the JS->wasm boundary, at least not when invoked via the `vitest` integrated runner for running a single test. Setting a breakpoint seems to work OK, but the debugger integration isn't working so hot after that: none of the C++-y things seem to be working (lots of `use of undeclared identifier`)

Still not clear why/how the stderr thing happened; I'm not setting the entrypoint (oops?) but that should be getting me an ERROR about that, not a failure to parse 'emsc' (where's that even coming from?).

Oh, it's because of this:

```
diff --git a/content/talvos/fill.test.ts b/content/talvos/fill.test.ts
index 963e2e1..e8e12c7 100644
--- a/content/talvos/fill.test.ts
+++ b/content/talvos/fill.test.ts
@@ -34,7 +34,7 @@ describe('fill', async () => {
 		// which means everything is `T | undefined`
 		return p.then((instance): [(...args: any[]) => any, (...args: any[]) => any] => [
 			instance.cwrap('validate_wasm', 'boolean', ['string']),
-			instance.cwrap('test_entry', 'void', ['string'], ['string']),
+			instance.cwrap('test_entry', 'void', ['string', 'string']),
 		]);
 	}(talvos({
 		print: (text: any) => { stdout += text; },
```

So, uh, yeah: "don't get the interface wrong" I guess?

## iterating on fill.spvasm

AKA testing the test.

Now that I've got the fill test set up to run in "continuous mode" (which requires starting a "continuous mode run" from the left-hand panel), it's time to see if I can strip away any more of the SPIR-V bits and still end up with a working program.

It seems kinda strange that the vector to fill would be a global (/ static?) pointer, instead of a function argument. That being the case for the (thread-local) GlobalInvocationId makes more sense (although those names are confusing af).

Also, how many layers of indirection are happening to get access to the invocation id?

```
%24 = OpAccessChain %19 %2 %const_0
%25 = OpLoad %uint32_t %24
```

  OpAccessChain : https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpAccessChain

so %24 is the address for a value of type %19, indexed from a compound value starting at the base (%2) with the remainder of the arguments as the operands. Kinda like https://llvm.org/docs/GetElementPtr.html

# Documentation

Where are we gonna put it? Ideally, somewhere like `docs/`, right?

Astro's [starlight] seems to mostly fit the bill. It offers a simple navigation skeleton, internationalization, some halfway decent looking code blocks, and is pretty easy to get set up.

One concern is that it seems to want to "own" the entire site: more "this is the documentation site, with one or two overrides/exceptions" than "this is a way to add documentation to an existing site." See also: [./content/docs/_FIXME.md](./content/docs/_FIXME.md). It seems to work for now, though.

[starlight]: https://starlight.astro.build/

# Control Kernel

- [ ] printing from `main` ? the pipes thing?
- [ ] error handling (similar to ^) ? We can't use OpTerminate b/c we're not a fragment (??)
- [ ] (broadly) support for "dynamic parallelism" in talvos, i.e. kernels launching other kernels

## The SPIR-V native way (/ secretly: OpenCL)

Implementing capabiltiy `DeviceEnqueue` implies all of:

```
OpTypeDeviceEvent ; OpTypeQueue
OpEnqueueMarker ;
OpEnqueueKernel ; OpGetKernelNDrangeSubGroupCount ;
OpGetKernelNDrangeMaxSubGroupSize ; OpGetKernelWorkGroupSize ;
OpGetKernelPreferredWorkGroupSizeMultiple ;
OpRetainEvent ; OpReleaseEvent
OpCreateUserEvent ; OpIsValidEvent ; OpSetUserEventStatus ;
OpCaptureEventProfilingInfo ;
OpGetDefaultQueue ; OpBuildNDRange
```

(and they have the OpenCL API mis-features/leaks, i.e. `OpGetDefaultQueue` might return `null` if the queue hasn't been created yet [?!], there's no non-default queues[??])

Wow, [`OpEnqueueKernel`](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpEnqueueKernel) looks an awful lot like [`clEnqueueNDRangeKernel`](https://registry.khronos.org/OpenCL/sdk/3.0/docs/man/html/clEnqueueNDRangeKernel.html)

So, getting this working would take:

- [ ] implement minimal subset
    - [ ] `OpTypeDeviceEvent` & `OpTypeQueue`
    - [ ] `OpBuildNDRange` ? (probably not strictly, but it would be convenient)
    - [ ] `OpGetDefaultQueue` (or some ~equivalent/builtin extension?)
    - [ ] `OpEnqueueKernel`
- [ ] how to wait in main?
  - https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#_kernel_enqueue_flags -> define when the child may launch w.r.t. the parent
  - maybe ? https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpGroupWaitEvents
  - and/or ? https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpControlBarrier

## talvos-specific SPIR-V extensions (?)

Instead, we could define the semantics to always block, eschewing the "queue" notion; combined with an implied memory barrier we could avoid confusion around different storage classes/implied cache coherency w.r.t. memory accesses. Also potentially gives us a convenient measurement hook for identifying the difference between the `main` kernel (which probably isn't going to be particularly efficient) and the target functionality.

The main risks are the converse of the above; the asynchrony is what permits optimizations CPUs can't implement thanks to path dependence. By giving the Talvos simulator a more CPU-like magic mode, we muddy the waters a bit and possibly teach/develop patterns that are too expensive to implement in practice.

Q: how does CUDA/`ptx` express this idea? i.e. what does an in-kernel invocation of another kernel compile down to, in CUDA-land?
  - If we're able to identify & express those semantics, it might be a more attractive candidate for upstreaming eventually
  - https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#independent-thread-scheduling <-- sort of relevant; late model nvidia GPUs now have PC (& stack) per thread, not just "warp" (core)
  - ah, here it is: https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#device-side-launch-from-ptx
  - and here's some semantics:
  - https://developer.nvidia.com/blog/cuda-dynamic-parallelism-api-principles/
    > Grids launched with dynamic parallelism are fully nested. This means that child grids always complete before the parent grids that launch them, even if there is no explicit synchronization
    -> implies that the invocations are (roughly) structured
  - there's still some wrinkles, though, as in: https://stackoverflow.com/a/30794088
    > the GPU will schedule the highest priority work but it will not pre-empt any running blocks. If the GPU is full then the compute work distribution will not be able schedule the child blocks [...] If the block that launched the work does a join operation (cudaDeviceSynchronize) [it] will pre-empt itself. The CDP scheduler will restore the parent block when the child grid has completed.
  - Looks like closer to a talvos "invoke" extension than the opencl model


Implementing this would require:

- [x] ~~`OpExtension "talvos_rt"`~~ `OpExtension "SPV_TALVOS_dispatch"`
  - NB: not `%talvos = OpExtInstImport "talvos.rt.v1"`, because `OpExtInstImport` is (apparently) old & busted
  - see: https://github.com/KhronosGroup/SPIRV-Guide/blob/main/chapters/extension_overview.md
  - cf. https://github.com/KhronosGroup/SPIRV-Registry
  - & https://github.com/KhronosGroup/SPIRV-Guide/blob/main/chapters/creating_extension.md
  - In fact, SPIRV-Tools is very much _not_ set up to accept any "out of band" extensions, they all are intended to go through the registry.
- [x] `OpDispatchTALVOS ...`
  - NB: not `%R = OpExtInst %R_ty %talvos <X> operand0, operand1, ... `, that's part of `OpExtInsImport`
- [ ] defining operands (& overloading?)
- [ ] constructing an example mapping back to the ocl semantics for other "client environments"


### adding an extension, spirv-side

Getting `spirv-tools` to recognize the new extension/opcode enough that Talvos could even see it took some doing; in order to experiment without needing to achieve prior consensus, we forked the SPIRV-Headers & SPIRV-Tools repos (although the latter could _probably_ have been avoided by simply ignoring `DEPS`)

see ./hack/misc/spirv-add-ext.sh for a partially worked example


### adding an extension, talvos-side

The first steps are "chasing the Unimplemented abort"; e.g.

```
Unimplemented extension SPV_TALVOS_dispatch
```
->
```diff
@@ -341,7 +355,8 @@ public:
         if (strcmp(Extension, "SPV_KHR_8bit_storage") &&
             strcmp(Extension, "SPV_KHR_16bit_storage") &&
             strcmp(Extension, "SPV_KHR_storage_buffer_storage_class") &&
-            strcmp(Extension, "SPV_KHR_variable_pointers"))
+            strcmp(Extension, "SPV_KHR_variable_pointers") &&
+            strcmp(Extension, "SPV_TALVOS_dispatch"))
         {
           std::cerr << "Unimplemented extension " << Extension << std::endl;
           abort();
```

After plumbing:

1. the capability
2. the extension
3. the opcode

We can successfully crash with e.g.

```c++
void Invocation::executeDispatch_Talvos(const Instruction *Inst)
{
  std::cerr << "oh my god it's been a long road. but we're finally home."
            << std::endl;
  abort();
}
```

### Shipping the change

```
git -C wasm/talvos commit -av
git -C wasm/SPIRV-Headers commit -av
# edit the wasm/SPIRV-Tools/DEPS file to point to the commit ^
git -C wasm/SPIRV-Tools commit -av
git add -u
git submodule foreach git push
git ci -av
git push
```

### Implementing the actual dispatch

First target: just statically invoke the FILL kernel 16x1x1 on the same device.

PipelineExecutor thinks of itself as running a single task at a time, so duplicating that and accessing private fields (OOPs, amirite?) successfully gets us to:

```
Aborted(Assertion failed: Id.X < GroupSize.X * NumGroups.X && Id.Y < GroupSize.Y * NumGroups.Y && Id.Z < GroupSize.Z * NumGroups.Z, at: ../wasm/talvos/lib/talvos/PipelineExecutor.cpp,2313,doSwtch)
```

Is this "device is full" message? Let's try a smaller invocation; nope, still doesn't like it.

Ah, it's because PipelineExecutor is chock full o' static & thread_local data, right. So we push a

hmm, C++

```c++
  talvos::PipelineStage *Stage = new talvos::PipelineStage(
      Dev, CurrentModule,
      CurrentModule->getEntryPoint("FILL", 5 /*EXEC_MODEL_GLCOMPUTE*/), {});

  talvos::ComputePipeline ComputePipeline(Stage);
```

vs.

```c++
  talvos::PipelineStage Stage = new talvos::PipelineStage(
      Dev, CurrentModule,
      CurrentModule->getEntryPoint("FILL", 5 /*EXEC_MODEL_GLCOMPUTE*/), {});

  talvos::ComputePipeline ComputePipeline(&Stage);
```

with

```c++
ComputePipeline::~ComputePipeline() { delete Stage; }
```

(in fairness, the comment _did_ say that ownership will be transferred)

## revising to be (more) correct

The semantics we require are:

1. The data setup "completes" (e.g. tasks exit, memory effects are visible, and ...? ) before launching the "target" kernel

2. We do not "overpromise" concurrency of the parent
   > The child grid is only guaranteed to return at an implicit synchronization.

3. We get the memory model right:
   > Parent and child grids share the same global and constant memory storage, but have distinct local and shared memory.

We'd also like to preserve the future possibility of:

1. Concurrency between the setup kernels
2. The ability to inspect the state _after_ the "target" kernel completes


hmm, thoughts on "dispatch" vs. "tail launch"
  if we "tail launch" into the target from setup, which gets invoked from main, then are we good? yes, but not when it comes time to print things.


it seems like a "stream" is a common notion, here; and since all work executes in-order in a stream, we could do something like:

  launch <S> BEFORE   -> e.g. FILL
  launch <S> "target" -> `vecadd`
  launch <S> AFTER    -> DUMP
  exit main

which means that the "exit main" will block until S completes, which and (working backwards) printing in AFTER will "see" the work done by the target kernel which "sees" the work from BEFORE

hmm, this feels _almost_ right; in fact, we'd like the launch operation to be itself somewhat atomic, e.g. as if we inserted a marker event that gets automatically cleaned up when main exits........... oh, yeah, that's the "tail queue".

Anyway, we want that, because otherwise (given 2x8 cores-by-lanes):

   START: main 1x1x1 -> leaves 1x8
    LAUNCH: FILL 16x1x1 -> schedules 1x8 of the FILL "right away"
    LAUNCH [concurrent w/ FILL]: SERIES 16x1x1 -> main has exited? runs at 2x8; otherwise (because we still have 1x8 of FILL to run) runs at 1x8 then 1x8
    LAUNCH [concurrent...]: vecadd 16x1x1 -> *same as above*; topology differs depending on amount/scheduling of setup work
    ...
  END: main -> frees up to 2x8

if we only implement so called "tail launches" for now, that defers the launches until after the main exits, so:

  START: main 1x1x1 -> leaves 1x8
    LAUNCH FILL -> but doesn't start yet
    LAUNCH SERIES -> but doesn't start yet
    LAUNCH vecadd -> but doesn't start yet
    LAUNCH DUMP -> but doesn't start yet
  END: main -> frees up to 2x8
  RUN: FILL @ 2x8
  RUN: SERIES @ 2x8
  ...

in other words, it removes non-determinism from scheduling.


## Scheduling

TODO residency & threads

### Dynamic Parallelism

> The invocation and completion of child grids is properly nested, meaning that the parent grid is not considered complete until all child grids created by its threads have completed, and the runtime guarantees an implicit synchronization between the parent and child.
 — https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#parent-and-child-grids


> Execution of a grid is not considered complete until all launches by all threads in the grid have completed. If all threads in a grid exit before all child launches have completed, an implicit synchronization operation will automatically be triggered.
 — https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#parent-and-child-grids

> CUDA runtime operations from any thread, including kernel launches, are visible across all the threads in a grid.
 — https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#parent-and-child-grids

> As long as the outstanding launch count is less than the queue depth, the launch process will be asynchronous.
 — https://stackoverflow.com/questions/53970187/cuda-stream-is-blocked-when-launching-many-kernels-1000


> On compute capability 3.5 - 5.x the GPU will schedule the highest priority work but it will not pre-empt any running blocks. If the GPU is full then the compute work distribution will not be able schedule the child blocks. As parent blocks complete the child blocks will be distributed before any new parent blocks. At this point the design could still dead lock.
 ­— https://stackoverflow.com/questions/30779451/understanding-dynamic-parallelism-in-cuda/30794088#30794088


#### Deadlock Detection

>     1. Mutual exclusion: At least one resource must be held in a non-shareable mode (we are assuming that one resource could have multiple instances); that is, only one process at a time can use the resource. Otherwise, the processes would not be prevented from using the resource when necessary. Only one process can use the resource at any given instant of time.[8]
>     2. Hold and wait or resource holding: a process is currently holding at least one resource and requesting additional resources which are being held by other processes.
>     3. No preemption: a resource can be released only voluntarily by the process holding it.
>     4. Circular wait: each process must be waiting for a resource which is being held by another process, which in turn is waiting for the first process to release the resource. In general, there is a set of waiting processes, P = {P1, P2, ..., PN}, such that P1 is waiting for a resource held by P2, P2 is waiting for a resource held by P3 and so on until PN is waiting for a resource held by P1.
>
> These four conditions are known as the Coffman conditions from their first description in a 1971 article by Edward G. Coffman, Jr.
— https://en.wikipedia.org/wiki/Deadlock

So here, the (1) mutually exclusive resource ought to be: "excution" (block? "workgroup"? what're we calling this). Condition (2) arises from the "all of my children must complete before I can complete" (so the parent "holds" the resource the child is requesting). Condition (3) holds unless legacy `cudaDeviceSynchronize()` or (... something CM6 pre-emption? ... ); so detection falls down to (4), for cycles—which there will be just in case there is unscheduled child work and the parent gets to a sync point that prevents pre-emption (?? not exit, but... waiting on a signal or something? blocking on the stream? [unless that pre-empts?])

#### Ordering

> The ordering of kernel launches from the device runtime follows CUDA Stream ordering semantics. Within a grid, all kernel launches into the same stream (with the exception of the fire-and-forget stream discussed later) are executed in-order.

Q: What are "CUDA Stream ordering semantics?" (is there any more there than the next sentence?)

Executing in-order means launch queue, sure. But what about when multiple concurrent launches occur? Ah, next sentence:

> With multiple threads in the same grid launching into the same stream, the ordering within the stream is dependent on the thread scheduling within the grid, which may be controlled with synchronization primitives such as __syncthreads().

Dependent on thread scheduling makes our life easier (or much harder lol), because (1) that "just" means we push into the queue in whatever order we execute, and/or (2) that's an "arbitrary choice" point which turns into some NP-like search space.

> Note that while named streams are shared by all threads within a grid, the implicit NULL stream is only shared by all threads within a thread block. If multiple threads in a thread block launch into the implicit stream, then these launches will be executed in-order. If multiple threads in different thread blocks launch into the implicit stream, then these launches may be executed concurrently.

(hmm, probably shouldn't have that distinction / a "NULL" stream)

>  If concurrency is desired for launches by multiple threads within a thread block, explicit named streams should be used.

TODO worth making "setup" concurrent?
  there _is_ useful parallelism there, most of the time
  possibly: induces need for race detection?


> There is no guarantee of concurrent execution between any number of different thread blocks on a device.

(I think they mean "parallel" execution here: there's no guarantee of parallelism, but concurrency is the property that's being expressed)
