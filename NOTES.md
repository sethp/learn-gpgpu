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
