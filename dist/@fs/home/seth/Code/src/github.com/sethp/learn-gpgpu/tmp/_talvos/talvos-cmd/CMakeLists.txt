# Copyright (c) 2018 the Talvos developers. All rights reserved.
#
# This file is distributed under a three-clause BSD license. For full license
# terms please see the LICENSE file distributed with this source code.

add_executable(talvos-cmd talvos-cmd.cpp
               CommandFile.cpp CommandFile.h)
target_link_libraries(talvos-cmd talvos)

if (EMSCRIPTEN)
    # see: https://github.com/emscripten-core/emscripten/pull/3948#issuecomment-744032264
    # TODO: generate the process.env.js file on demand?
    target_link_options(talvos-cmd PUBLIC
        -sNODERAWFS
        -sENVIRONMENT=node
        -sERROR_ON_WASM_CHANGES_AFTER_LINK
        -sWASM_BIGINT
        --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/process.env.js
        -sINCOMING_MODULE_JS_API=['preRun']
    )

    # TODO can we rename this file on output ? talvos.wasm would be cooler than talvos-wasm.wasm
    # eh, not really... maybe if we move this into libtalvos and don't have it be "executable"
    # (but then we'd need our own loading infrastructure, or at least tickle emscripten to do that for us...)
    add_executable(talvos-wasm wasm.cpp CommandFile.cpp CommandFile.h)
    target_link_libraries(talvos-wasm talvos)

    target_compile_options(talvos-wasm PUBLIC -O0) # TODO -Og breaks emscripten exceptions lol ?
    #target_compile_options(talvos-wasm PUBLIC -fexperimental-new-constant-interpreter)


    # TODO STANDALONE_WASM ?  https://github.com/emscripten-core/emscripten/blob/5b24a09de582f3943d75aa6af9be36ba719c08c0/src/settings.js#L1366
    target_link_options(talvos-wasm PUBLIC
        -sMODULARIZE
        # -sWASM_WORKERS=1
        -sENVIRONMENT=web,node
        -sEXPORT_NAME=talvos
    )
    # mutually exclusive with the above
    # set_target_properties(talvos-wasm
    #     PROPERTIES
    #         SUFFIX ".html"
    # )
    # target_link_options(talvos-wasm PUBLIC
    #     -sPROXY_TO_WORKER=1
    # )
    set(EXPORTED_RUNTIME_METHODS
        cwrap

        getExceptionMessage
        decrementExceptionRefcount

        wasmExports
        ccall

        stackAlloc
        stackSave
        stackRestore
        stringToUTF8OnStack
        writeArrayToMemory

        stringToUTF8Array
        UTF8ArrayToString

        wasmMemory
    )

    string(REPLACE ";" "," EXPORTED_RUNTIME_METHODS "${EXPORTED_RUNTIME_METHODS}")
    target_link_options(talvos-wasm PUBLIC
        -sASSERTIONS=1
        -sEXPORT_ES6=1
        -sEXPORTED_FUNCTIONS=_malloc,_free
        -sEXPORTED_RUNTIME_METHODS=${EXPORTED_RUNTIME_METHODS}
        -sEXPORT_EXCEPTION_HANDLING_HELPERS=1 # TODO `decrementExceptionRefcount' was not exported (+`getCppExceptionTag`)
        -sALLOW_BLOCKING_ON_MAIN_THREAD=0
        --emit-tsd "$<TARGET_FILE_DIR:talvos-wasm>/talvos-wasm.d.ts"
    )

    target_link_options(talvos-wasm PUBLIC -sERROR_ON_WASM_CHANGES_AFTER_LINK -sWASM_BIGINT)
    target_link_options(talvos-wasm PUBLIC -Og)

    # Generates source maps; however, in Chrome (which has support for DWARF), the less-capable
    # sourcemap overrides the .dwp (dwarf package) if it exists.
    # Might still be useful for e.g. Firefox, Safari, but then we need to serve a different wasm
    # binary based on user agent (the binary encodes the source map link)

    # find_program(LLVM_DWARFDUMP NAMES llvm-dwarfdump PATHS /emsdk/upstream/bin/)
    # add_custom_command(OUTPUT "$<TARGET_FILE_BASE_NAME:talvos-wasm>.wasm.map"
    # # this doesn't re-run when wasm-sourcemap.py changes :-/
    # add_custom_command(TARGET talvos-wasm POST_BUILD
    #     COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/wasm-sourcemap.py "$<TARGET_FILE_BASE_NAME:talvos-wasm>.wasm" --dwarfdump=${LLVM_DWARFDUMP} -o "$<TARGET_FILE_BASE_NAME:talvos-wasm>.wasm.map" -u "$<TARGET_FILE_BASE_NAME:talvos-wasm>.wasm.map" -w "$<TARGET_FILE_BASE_NAME:talvos-wasm>.wasm"
    #     # BYPRODUCTS "$<TARGET_FILE:talvos-wasm>.map"
    #     VERBATIM
    # )

    find_program(EMDWP emdwp)
    add_custom_command(
        TARGET talvos-wasm POST_BUILD
        # we have to name it .wasm.dwp , because that's the discovery methodology (just try it)
        # https://github.com/ChromeDevTools/devtools-frontend/blob/6f2433034d59c82daee92dca3737afec0762e466/extensions/cxx_debugging/src/MEMFSResourceLoader.ts#L18
        COMMAND ${EMDWP}
            -e $<TARGET_FILE_DIR:talvos-wasm>/$<TARGET_FILE_BASE_NAME:talvos-wasm>.wasm
            -o $<TARGET_FILE_DIR:talvos-wasm>/$<TARGET_FILE_BASE_NAME:talvos-wasm>.wasm.dwp
        WORKING_DIRECTORY ${Talvos_BINARY_DIR} # maps to e.g. /usr/src/talvos/build/emscripten-docker
    )
endif()

install(TARGETS talvos-cmd DESTINATION bin)
