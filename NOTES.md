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


### Scheduling

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

## defining operands

We need to offer either 1) a specific opcode for tail-launches, or 2) take a "stream" parameter here that must always be "tail" (for now).

As of now, we know we need to take at least:

1. function selector ("FILL" or "SERIES"); entry point string or %func_id ?
  - is it fair to say that the difference between a "function" and an "entrypoint" is "who can call it?", i.e. a function that can be called _on_ the GPU from "outside" is an entrypoint, and an undecorated function cannot?

2. "group size", e.g. `16x1x1` (how does this map to CUDA's grid(s)/blocks/threads?); should that be an [execution mode] decoration (implies "entry point" above), or a literal, or an ID-based operand?
3. some mechanism for passing arguments (NB: pointers require special care)

[execution mode]: https://github.com/KhronosGroup/SPIRV-Guide/blob/main/chapters/entry_execution.md#execution-mode

examples:

### ~1:1 with the other launch APIs

```
OpDispatchTALVOS "Tail" "FILL" <1 1 1; 16 1 1> 128kiB <fn params...>
; or
OpDispatchTALVOS Tail %fill_fn %g_dim %b_dim %shm_sz <fn params...>
; or w/ `OpExecutionMode %fill_fn LocalSize 16 1 1` up at the "top"
OpDispatchTALVOS Tail %fill_fn %shm_sz <fn params...>
```

Breaks down as:

* `OpDispatchTALVOS`
* `"Tail"` or `Tail`: the stream (queue) name
    - This magic string maps to a special stream that does something uniqe: it launches after the exit of the currently running kernel. Currently, that's the only support target
    - there are two "special" streams, `NULL`/default (which are subtly different configurations) and the "tail" stream (only supported on NVIDA devices). Users may create streams (up to... ?), which are named, and share different semantics
    - The problem with `Tail` (a literal/enumerant) is that it would imply either 1) no ability to specify user streams, or 2) require overloading the operand, which can be quite confusing (i.e. if `Tail` or `NULL/``Default` meant the special streams, then `"Tail"`/`"NULL"`/`"Default"` would all mean a user-created stream that had no special semantics)
    - A string here is a faux pas in SPIR-V land; since we'll probably refer to the same stream more than once, the aesthetic is to be compact & reference a result id instead
    - That would require a separate opcode to set up, something like: `%nn = OpXXXTALVOS "Tail"` and/or `%nn = OpTailStreamTALVOS`. If we want to express the type for `%nn` as well, that's another e.g. `%nn_t = OpStreamTypeTALVOS`
* `"FILL"` (an entrypoint name) / `%fill_fn`
  - This string maps to an entrypoint name; nominally something in the user's control, but out-of-line (up at the "top" of the file where `OpEntryPoint ...`s are required to go)
  - Again, a string is kind of a faux pas, instead we ought to use `%fn` from a `%fn = OpFunction` declaration (which may be externally linked if decorated with "Linkage Attributes")
* `<1 1 1; 16 1 1>` (or `%g_dim %b_dim`): this is invalid SPIR-V syntax, but it represents `<grid/group dim; block dim>`
  - `group dim` is a "multiplier" on `blocks`; balancing out the three-way constraint triangle between work size and blocks/group dim is complicated. For small examples we'd prefer to only use `blocks` as a simplifying assumption, but the need for the second one comes up relatively quickly.
  - so, the choices here are kinda rough: `EnqueueDispatch` opts for using result ids which are constructed by e.g. `%block_dim = OpBuildNDRange %ndrange_ty ...` which requires a correctly-set-up struct type (via the usual typing opcodes) that obeys a whole lotta rules, populated by a bunch of out-of-line setup
  - Spending a bunch of opcodes _elsewhere_ to set things up is typical of a low-level operation-based language like SPIR-V; the short-term memory/inline hinting/symbolic manipulation demands are what makes assembly programming so challenging; it's just extra unfortunate here, because the number of operations it takes to express this core concept is way too high. It's possible to learn to answer the "dimensionality?" question by scanning for/jumping to the approximate "vector setup block" and pattern matching, but the size of the ask is a mismatch with the frequency of the task, and how early it needs to be performed (~immediately).

  Q: how to do vector constants in SPIR-V? Is there a more compact way than poking the values in one at a time?

  - an alternative is to decorate w/ `OpExecutionMode %fill_fn LocalSize 16 1 1` (and `GlobalSize` for the other dimension, only supported for `Kernel`s), and then this operand disappears entirely. This requires all dispach'd functions to be an EntryPoint as well (but: we probably have that requirement since AMD doesn't support dynamic/nested parallelism)

  - Using the decoration is a little funky, though: we'd be extending it in a very natural but also tons-of-work-to-get-working-right kind of way, and it's not at all clear that's something which can't be easily "lifted" out of Talvos
  Q: is this ^ right? What happens if we use `Kernel` instead of `Shader`?
    Seems to be fine, more or less—Talvos now knows about two kinds of compute-focused things to launch, but that's alright. The one big wrinkle is in figuring out how passing data into/back out of a `Kernel` is supposed to work?

    Ugh, except for this: https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#_aliasing

    The main thing the OpenCL memory model permits is aliasing-by-default. Hmmmm, time for a Talvos memory model?

  Q: do intel's GPUs support dynamic parallelism?

* `128kiB` / `%shm_sz`: the shared memory size (also probably invalid SPIR-V syntax)
  - it's clear why the runtime would need this "as soon as possible," but it's not clear whether this ought to be a per-dispatch tune-able (would it ever make sense to dispatch the same kernel with different sizes? _maybe_ if you were doing different dimensions, right?)

* `<fn params...>`: the ids (no literals) of all the parameters to the function
  - NB: any pointers passed here must be to the "global" storage class(es), since the dispatched kernel won't have access to any of the local/"shared" memory of the invoker

Does not cover:
- non-shared-memory configuration parameters; i.e. resizing limits L1 cache usage
- any (device-)global pointers

### ducking the "streams" parameter, for now

So, wrapping that up into the opcode (since it's a _special_ stream anyway)

```
; earlier: `OpExecutionMode %fill_fn LocalSize 16 1 1`

OpDispatchAtEndTALVOS %fill_fn <fn params...>
; or
OpDispatchDeferredTALVOS %fill_fn <fn params...>
; or
OpDispatchTailTALVOS %fill_fn <fn params...>
; or
OpDispatchOnExitTALVOS %fill_fn <fn params...>
; or
OpDispatchLaterTALVOS %fill_fn <fn params...>
```

We're down to the question of "how do we signal that this is dispatching into the special stream that has the 'nesting' semantics


### avoiding bad surprises with `OpExecutionMode`

_not_ to be confused with "execution model"

Rather than:

```
OpEntryPoint GLCompute %fill_fn "FILL" %gl_GlobalInvocationID
OpExecutionMode %fill_fn LocalSize 16 1 1
```

We might have a better time with something like:

```
OpKernelTALVOS %fill_fn "FILL" %gl_GlobalInvocationID 16 1 1
```

we could even do length-extended overloading (which is "ok kind of overloading") to handle the other sizing too

TODO is that just wrapping two opcodes in another opcode? is that worth doing?
TODO elsewise, write a test for `OpExecutionModeId` (dynamic paralellism lol)


### big oof

```
Initializer
Indicates that this entry point is a module initializer.
```

&

```
Finalizer
Indicates that this entry point is a module finalizer.
```

I wonder if that could take the place of the dispatch op.

## `OpExecutionGlobalSizeTALVOS` (stepping back from full dispatch for a moment)

Instead, let's try doing something smaller and adding a peer of `OpExecutionMode` for setting the global size, called ~~`OpGlobalSizeTalvos`~~ `OpExecutionGlobalSizeTALVOS`

Did the same as above to add it to the spirv.core.grammar.json, but then the validation started failing. First it was something ~ ID has not yet been declared, then ~ must be in a block, then finally:

```
error: 7: Invalid use of function result id '1[%1]'.
  OpExecutionGlobalSizeTALVOS %1 16 1 1
```

For each, searching for the message (e.g. "Invalid use of function result id") would yield a block of code, like:


```c++
  for (auto& pair : inst->uses()) {
    const auto* use = pair.first;
    if (std::find(acceptable.begin(), acceptable.end(), use->opcode()) ==
            acceptable.end() &&
        !use->IsNonSemantic() && !use->IsDebugInfo()) {
      return _.diag(SPV_ERROR_INVALID_ID, use)
             << "Invalid use of function result id " << _.getIdName(inst->id())
             << ".";
    }
  }
```

and then it was just a matter of taking a different branch, i.e. adding `spv::Op::OpExecutionGlobalSizeTALVOS` to the end of the "acceptable" declaration:

```diff
diff --git a/source/val/validate_function.cpp b/source/val/validate_function.cpp
index 639817fe..9bd52993 100644
--- a/source/val/validate_function.cpp
+++ b/source/val/validate_function.cpp
@@ -86,7 +86,8 @@ spv_result_t ValidateFunction(ValidationState_t& _, const Instruction* inst) {
       spv::Op::OpGetKernelPreferredWorkGroupSizeMultiple,
       spv::Op::OpGetKernelLocalSizeForSubgroupCount,
       spv::Op::OpGetKernelMaxNumSubgroups,
-      spv::Op::OpName};
+      spv::Op::OpName,
+      spv::Op::OpExecutionGlobalSizeTALVOS};
   for (auto& pair : inst->uses()) {
     const auto* use = pair.first;
     if (std::find(acceptable.begin(), acceptable.end(), use->opcode()) ==
```

At this point we're back in the "Unimplemented ..." pipe (as above).

## `OpBufferTALVOS`

Idea is to replace:

```tcf
BUFFER a 64 UNINIT
DESCRIPTOR_SET 0 0 0 a

# ...

DUMP UINT32 a
```

and

```spirv
OpDecorate %buf0 DescriptorSet 0
OpDecorate %buf0 Binding 0

; ...

%_arr_uint32_t = OpTypeRuntimeArray %uint32_t
%_arr_StorageBuffer_uint32_t = OpTypePointer StorageBuffer %_arr_uint32_t

; ...

%buf0 = OpVariable %_arr_StorageBuffer_uint32_t StorageBuffer
```

with something like:

```spirv
%_arr_uint32_t = OpTypeRuntimeArray %uint32_t

%buf0 = OpBufferTALVOS 64 %_arr_uint32_t StorageBuffer "a"
```

and have Talvos automagically DUMP all (named) buffers after execution.

TBD:

- [x] Should we have the %_arr_StorageBuffer_uint32_t type as well?
  - [~] Or can we build that up from the `OpBufferTALVOS` arguments? (should we?)
- [ ] maybe we have an `OpBufferTypeTALVOS` ? Or a `BufferTALVOS` storage class?

We do "need" it, because else we see:

```
error: 25: The Base <id> '13[%13]' in OpAccessChain instruction must be a pointer.
  %17 = OpAccessChain %_ptr_StorageBuffer_uint %13 %16
```

so something needs to be an `OpTypePointer`, and it's probably not worth overloading the whole result type machinery to special case just `OpBufferTALVOS` to return a pointer-wrapped type.

We still might want an `OpBufferTypeTALVOS` and/or a special storage class; those would both restrict the type argument in about the same way, so it's not clear what the buffer type would give us.

The main benefits of being explicit here is:
1. We can invoke it with some capability other than `Shader`
2. It's less surprising than overloading SharedBuffer with dump behavior (?), and it's a trivial remapping to change to the SharedBuffer storage class get it working outside Talvos.

And potentially:

3. We might add an optional flags parameter to control talvos-specific behaviors; too soon to say if that's really useful though.


- [ ] should we leave the `OpVariable` thing as-is ...
  - [ ] and just decorate the buffer with a (mostly) non-semantic `OpBufferTALVOS` ?
  - [ ] and just literally decorate with an entirely non-semantic `OpDecorate %buf0 BufferTALVOS` ?

Well, we had to fudge the order, at least, and will probabably have to do the `_StorageBuffer_` type bits. Too bad, `StorageBuffer` requires `OpCapability Shader` & is kind of semantically redundant.

Perhaps instead, a `BufferTALVOS` _StorageClass_ w/ `OpName %... "a"` ?

### (sort of) aside: what the heck is a `OpAccessChain` ?

```
; given %buf0 ty is `uint32_t[]*` (in StorageBuffer)
; and %3 is a uint32_t offset
%4 = OpAccessChain %_ptr_StorageBuffer_uint32_t %buf0 %3
```

%4 is a ptr to a uint32_t, aka `uint32_t*`, offset into the _array_ by %3 "steps"? .... how?

Ok, so if `buf0` is `0x1000`, this breaks down to roughly:

   0x1000     ; "base"
  +  (4       ; sizeof(uint32_t)
      * %3)   ; element-wise offset
  ---------
   0x103c     ; when %3 == 15

Which, when interpreted as a `uint32_t *` sure could be right...

why does this feel weird? because `uint32_t[]*` ought to be an alternate spelling of `uint32_t**`, which means we ought to have something like `0x1040` in `buf0`, which points to a 8-wide slot containing `0x1000`; so maybe OpAccessChain contains an implicit deref on its first argument? i.e. it's not `(base) + offset`, it's `*(base) + offset`?

oh, wait, no: `uint32_t[][]` is different from `uint32_t[]*` which is a different beast than `uint32_t**` (https://stackoverflow.com/questions/917783/how-do-i-work-with-dynamic-multi-dimensional-arrays-in-c#comment729159_918121) ; the C standard requires `[][]` to be contiguous, but obviously `**` has no such requirement.


# visualizing wide vector processors

A single "slot" in a SIMT vector computation brings together all of:

1. An operation
1. A mask
1. (vector) register operands
1. An implicit "offset" (and/or "base" ?)

Sometimes the register is sourced from memory, as in the result of a previous `OpLoad`.

  e.g. https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#special-registers

    %tid is a 4"d" vector (4 components, but the 4th component is always 0)
      in `FILL_IDX`, one of its components is what we want to fill the value with (usually the first)

      ranges from [ 0, 0, 0 ] to [ %ntid.x ,  %ntid.y ,  %ntid.z ]

    distinct from %warpid (which is the "physical" id "inside" the "core")

Is there anything else? Let's work some examples:

  FILL_IDX (type erased)

      %n = OpConstant %uint32_t 0

      ; ex. 1

      %2 = OpAccessChain <ty> %gl_GlobalInvocationID %n
      %3 = OpLoad <ty> %2

      ; ex. 2
      %4 = OpAccessChain <ty> %buf0 %3
           OpStore %4 %3

## operands

%gl_globalInvocationID points to a magical memory area that's distinct per-lane (the "base"/"offset") and roughly laid out like:

```
  core | | lane offset ->
offset v |           0              1
     O     [ x  y  z ]    [ x  y  z ]
```

in "3d" (w/ 32-byte values for each of the components)

So for %2, we're taking the component (%n) and adding it to the base to get the x/y/z "dim".

What does that mean in a SIMT context? Two interpretations:

1. %gl_GlobalInvocationID is "dynamically" derived as ("local" mem block base) + some offset specific to the variable itself; i.e. for 16-byte (4x4 byte) coordinates relative to a thread mem base of 0x1000 this might look like:

  @ core 0 ~ tp vreg is
    [
      0x1000    # lane 0
      0x1010    # lane 1
      0x1020
      ...
    ]

1. (talvos current model) %gl_GlobalInvocationID is a lane ("invocation")-specific memory address: reads from it produce different values, lane-to-lane.


This seems to be roughly the differnce between PTX's ["generic addressing"][ptx-gen-addr] and

[ptx-gen-addr]: https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#generic-addressing

From the former:

  > The state spaces [...] are modeled as windows within the generic address space [...] defined by a window base and a window size that is equal to the size of the corresponding state space. A generic address maps to global memory unless it falls within the window for const, local, or shared memory. [...] a generic address maps to an address in the underlying state space by subtracting the window base from the generic address.

(or, conversely, the address for a given state space maps to the global addressing scheme by adding the window base)

From the latter:

  > The address is an offset in the state space in which the variable is declared.

(good ol' segmented addressing comes back again lolol)

So, yeah: we can think of there being a lane-specific "base" (a few, actually, one per "state space"/"storage class") that gets added to the address when loading.

  Note: Talvos actually does model this, just not user-visibly; since it allocates a new "private" memory structure for each invocation, we could conceptually treat the base of _that_ object as (approximately) the segmented address base.

*Q*: why does this matter / what is it modeling?
  It appears that we're doing this for cache coherence reasons & read/write coalescing; if we have to be explicit about which writes have what visiblity relative to other lanes/cores, there's a pretty natural mapping to L1/L2/L3 caches from there, with no requirement for those caches to be in any way coherent.

*Q*: compare/contrast a lane-focused model of "thread registers" and "local result values" etc. vs. an e.g. core-specific model of vector registers ?


  - how memory do work?

    like, when I want to go populate a whole vreg in a batch access, there's either:

    1. issue one (masked) strided vector load (rvv takes this approach: https://rvv-isadoc.readthedocs.io/en/latest/load_and_store.html )

      i.e. make an explicit request to the memory subsystem to populate the whole register in a batch load, so it knows 1) the start, 2) the total size, and 3) the stride (and thus, alignment?) all at once.

    2. issue a buncha little individual loads, and expect someone else to observe the pattern and coalesce them into a strided memory access of a particular width (this seems to be ~ the model that PTX takes; kinda-sorta SPIR-V too, but SPIR-V also has decorations for strides so that's a thing)


    Oh, I see, this is a whole _thing_: https://stackoverflow.com/questions/56966466/memory-coalescing-vs-vectorized-memory-access

    Very interesting all around, but especially some good comments:

      > Characterization of memory coalescing as a "runtime optimization" isn't really correct. It is much more like the default execution model of the GPU. Divergence is what happens whenever an instruction can't be executed in lock-step fashion across a warp and that is what happens when a memory instruction can't be serviced in a single transaction
      — https://stackoverflow.com/questions/56966466/memory-coalescing-vs-vectorized-memory-access#comment100471270_56966466

      &

      > The memory controller looks at the actual addresses and determines which can be grouped together into specific cachelines or memory segments, and then issues the requests to those lines/segments. Since the addresses indicated by each thread in the warp cannot be known until runtime (in the general case) this activity cannot be pre-computed at compile time.
      — https://stackoverflow.com/questions/56966466/memory-coalescing-vs-vectorized-memory-access#comment100483129_56966466

    These, especially taken together, suggest to me that it's possible to "miss the train" for certain access patterns and have some of the data back for a subset of the operations sooner than the rest.

    But, crucially, that somehow the threads will continue to execute individually? (especially, apparently, on late model GPUs with "independent thread scheduling").

    That's a pretty big difference from a vector processing unit, where the load instruction won't retire until _all_ of the data's loaded.

      > Yes, memory-coalescing basically does at runtime what short-vector CPU SIMD does at compile time, within a single "core". That's because CPU SIMD is done very differently in general where you have wide fixed-width execution units, rather than GPU-style multiple execution units that can each handle one scalar, and be flexibly driven for data that's contiguous or not.
      — https://stackoverflow.com/questions/56966466/memory-coalescing-vs-vectorized-memory-access#comment100479736_56968091

    So, yeah, there is a duality here: but also a significant difference between SIMT and SIMD. It might be worth doing a compare/contrast here?

      > I like to think of a Titan Xp as having only 120 physical cores, each operating on 128-byte vector registers [...] CUDA just gives us the illusion of having 3840 cores.
      —https://stackoverflow.com/questions/56966466/memory-coalescing-vs-vectorized-memory-access#comment100498411_56968091

      & rejoinder

      > Why illusion? They are physical. CUDA doesn't guantee to use all of them (see occupancy) because of restrictions (being available registers, one of them, if not the most important). So you have 120 SM, each with common load/store units capable to optimize memory accesses when instructions (by the mean of warps) access concurrently subsequent memory locations (and not only).
      —https://stackoverflow.com/questions/56966466/memory-coalescing-vs-vectorized-memory-access#comment100505179_56968091


    Both models are useful: the coarser "it's just a vector processor" and the finer "the load/store units can optimize (some) noncontiguous access patterns" (but cf. the CUDA C programming guide, which warns against non-unit stride accesses).

    So there's a priority question, here:


  - also, thread-local scheduling
    (but, perhaps now PC is just a vreg?)

## operations

For most operations ("instructions"), we can get away with using their result code

### (answered) How many no-result-id operations are there?

As of now, 113. (there are 736 total opcodes)

via `<./wasm/SPIRV-Headers/include/spirv/unified1/spirv.core.grammar.json jq -r '.instructions|map(select((.operands//[])|map(.kind=="IdResult")|any|not))|.[].opname' | sort`, they are:

```
OpAssumeTrueKHR
OpAtomicFlagClear
OpAtomicStore
OpBeginInvocationInterlockEXT
OpBranch
OpBranchConditional
OpCapability
OpCaptureEventProfilingInfo
OpCommitReadPipe
OpCommitWritePipe
OpConstantCompositeContinuedINTEL
OpControlBarrier
OpControlBarrierArriveINTEL
OpControlBarrierWaitINTEL
OpCooperativeMatrixStoreKHR
OpCooperativeMatrixStoreNV
OpCopyMemory
OpCopyMemorySized
OpDecorate
OpDecorateId
OpDecorateString
OpDecorateStringGOOGLE
OpDemoteToHelperInvocation
OpDemoteToHelperInvocationEXT
OpDispatchTALVOS
OpEmitMeshTasksEXT
OpEmitStreamVertex
OpEmitVertex
OpEndInvocationInterlockEXT
OpEndPrimitive
OpEndStreamPrimitive
OpEntryPoint
OpExecuteCallableKHR
OpExecuteCallableNV
OpExecutionGlobalSizeTALVOS
OpExecutionMode
OpExecutionModeId
OpExtension
OpFinalizeNodePayloadsAMDX
OpFunctionEnd
OpGroupCommitReadPipe
OpGroupCommitWritePipe
OpGroupDecorate
OpGroupMemberDecorate
OpGroupWaitEvents
OpHitObjectExecuteShaderNV
OpHitObjectGetAttributesNV
OpHitObjectRecordEmptyNV
OpHitObjectRecordHitMotionNV
OpHitObjectRecordHitNV
OpHitObjectRecordHitWithIndexMotionNV
OpHitObjectRecordHitWithIndexNV
OpHitObjectRecordMissMotionNV
OpHitObjectRecordMissNV
OpHitObjectTraceRayMotionNV
OpHitObjectTraceRayNV
OpIgnoreIntersectionKHR
OpIgnoreIntersectionNV
OpImageWrite
OpInitializeNodePayloadsAMDX
OpKill
OpLifetimeStart
OpLifetimeStop
OpLine
OpLoopControlINTEL
OpLoopMerge
OpMaskedScatterINTEL
OpMemberDecorate
OpMemberDecorateString
OpMemberDecorateStringGOOGLE
OpMemberName
OpMemoryBarrier
OpMemoryModel
OpMemoryNamedBarrier
OpModuleProcessed
OpName
OpNoLine
OpNop
OpRayQueryConfirmIntersectionKHR
OpRayQueryGenerateIntersectionKHR
OpRayQueryInitializeKHR
OpRayQueryTerminateKHR
OpReleaseEvent
OpReorderThreadWithHintNV
OpReorderThreadWithHitObjectNV
OpRestoreMemoryINTEL
OpRetainEvent
OpReturn
OpReturnValue
OpSamplerImageAddressingModeNV
OpSelectionMerge
OpSetMeshOutputsEXT
OpSetUserEventStatus
OpSource
OpSourceContinued
OpSourceExtension
OpSpecConstantCompositeContinuedINTEL
OpStore
OpSubgroupBlockWriteINTEL
OpSubgroupImageBlockWriteINTEL
OpSubgroupImageMediaBlockWriteINTEL
OpSwitch
OpTerminateInvocation
OpTerminateRayKHR
OpTerminateRayNV
OpTraceMotionNV
OpTraceNV
OpTraceRayKHR
OpTraceRayMotionNV
OpTypeForwardPointer
OpTypeStructContinuedINTEL
OpUnreachable
OpWritePackedPrimitiveIndices4x8NV
```

## a note on debuggers, breakpoints, and µarch

Where, microarchitecturally, does a breakpoint "occur"? Is it before the operation is issued, or before it is retired?

In the "all instructions are sequential-issued & sequentially-retired" model then it doesn't really matter, because either "before issue" or "before retire" seam is "before the effects are visible" and "after the previous instruction's effects are visible."

But, that requires precise exception handling in the µarch, because even in very simple pipelines there will be many in-flight instructions that have all been issued but have not yet retired when we hit our breakpoint.

In the five-stage RISC (AKA load-store) pipeline model of:

- fetch (IF)
- decode (ID)
- execute (EX, ALU)
- mem (MEM)
- writeback (WB)

we might say an instruction has been issued as soon as it's fetched, but that's only partially true for instructions following a branch: depending on the implementation (oh no!) an instruction is only guaranteed to be "issued" (i.e. make its effects "known") sometime after the dependent instructions leave the execute stage;  we're somewhat buffered by the load-store nature here because we can't branch on the contents of memory directly, and so we can get away with saying that EX is sufficient the instruction which depends on the memory will stall sometime before it leaves the execute stage.

## towards a new model

Perhaps we're going one level too deep to use the terminology "issued" and "retired," because 1) those are out-of-order execution terms, and 2) it still seems that GPUs conspire to make their pipelined operations still appear in-order depsite any dependencies—except when it comes to (some) exception precision. Ok, an alternative, then, is to create a purely sequential model (nice because it narrows limits the mental scope to a single operation at a time) where every operation is:

- not yet reached (AKA still in the future)
- not yet "satisfied" (previously, "retired")

So every computation is shuffling between three (exclusive) sets:

- all the not-yet-reached instructions (the ones "ahead" of the current "read head")
- all the reached-but-not-yet-"satisfied" instructions ("dispatched", previously "issued"?)
- all the instructions that are fully committed and whose effects are visible

So for the "read head" to reach a particular instruction is identical with "dispatch"ing it, since that's now the object under consideration. It may take some _resource_ (time? slots? bandwidth?) to satisfy the

Are there any standard terms? Seems like a no: https://stackoverflow.com/a/52763093/151464

Dive Into Systems[^dive-into-systems] seems like it avoids naming the set of actively executing instructions, but does use "complete" mostly consistently between its discussion of "single-slot" (i.e. non-pipelined, "clock-driven") execution [^dis-clock-driven] and pipelining[^dis-pipeline].

[^dive-into-systems]: https://diveintosystems.org/

[^dis-clock-driven]: https://diveintosystems.org/book/C5-Arch/instrexec.html#_clock_driven_execution

[^dis-pipeline]: https://diveintosystems.org/book/C5-Arch/pipelining.html

Ok, so prospective model then is:

- as soon as an execution unit ("core") encounters an operation, that operation is "dispatched"
- once an operation is "completed", its effects are visible and the execution unit atomically dispatches the next instruction

We might need to relax the atomic constraint in the future, but this model answers our current question of "when does the debugger pause"—_on_ the dispatch edge.

We can also say "the operation is dispatched once per lane simultaenously, and completed once per lane independently."

Seems OK: the words are a little long, but they're possibly less confusing than trying to pin down the more wriggly "issue" and "retire".

Some other samples:

- _dispatching_ a memory operation sends out its requests, and the operation can't _complete_ until those requests are satisfied by the memory unit
- an operation that has a _structural hazard_ will stall at the dispatch stage until the needed resource is available
- across multiple cores, how many operations have been dispatched?

The atomicity does confuse matters for SIMT-land, just a bit: the happens-before relationship between previous operation last-lane completion and next operation simultaneous dispatch isn't clear unless we invoke something that separates the two.

### spacing out dispatch and completion for learning

So, how do we model a gap there? The most natural place to throttle is at the "memory access" boundary; we might construct a toy GPU that has a tiny memory bandwidth which only allows it to load, say, 4 bytes at a time or so. Or maybe only allows it to "consider" one memory request at a time?

Let's play with that for now. All operations will complete in one "vis step" unless they touch memory, and then they compelete one-at-a-time (in order, to make it easier, though nondeterministically would be better).
