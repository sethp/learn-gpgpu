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

Will `git push` push both? (probably not)


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
