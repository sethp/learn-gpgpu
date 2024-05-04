#!/bin/bash

set -euo pipefail

[[ $# -gt 0 ]] || {
    echo >&2 "usage: $0 PATH"
    echo >&2 "missing required PATH to copy artifacts into once they're built"
    echo >&2 "e.g. $0 ../learn-gpgpu/wasm"
    exit 2
}

uniqifier() {
	# we use the inode of the tmpfile as part of the uniqifier
	# `mktemp` effectively reserves a name for our use, but
	# leaves a dangling file around unless we clean it up. using
	# the inode lets us do the "unlink an open file" trick, while
	# still reserving a unique name.
	#
	# see also: https://unix.stackexchange.com/a/181938
	#
	# why do this tortured, cursed thing? it handles:
	# * concurrent runs of this script[0]
	# * unexpected exits (even `kill -9`, after the rm)
	# * temp file "fixation" races
	#
	# remove when: docker provides a way to get the unique content sha
	# back out of a `docker build` other than `-q` which hides the output.
	#
	# [0] or indeed any script using the same docker repo prefix,
	#     as long as they share the same tmp directory or filesystem
	! [[ -e /proc/$$/fd/45 ]] || {
		# since bash 4.1 there's also a way to get the shell to allocate a fd for us,
		# but the default bash on _some_ platforms (macOS) is still version 3.
		# cf. https://stackoverflow.com/a/41620630
		echo >&2 "cannot generate uniqifier: file descriptor 45 already in use; did you call it twice?"
		echo >&2 "(if you need two unique tags, try making a \`uniqifier2\` copy and changing the file descriptor number)"
		exit 1
	}
	tmpfile=$(mktemp)
	exec 45<"$tmpfile"
	inode=$(stat -c'%i' "$tmpfile")
	rm "$tmpfile"

	echo "$(basename "$tmpfile")-ino-$inode"
}
TAG="talvos-build:$(uniqifier)"

# best effort; it'll be a while before the docker daemon will delete the layers anyway
# TODO: this is a little too eager, most of the time (even with --no-prune). I'd like
#       the image to stick around in case I want to get in and poke at something
# cleanup_tag() {
# 	docker image rm --no-prune "$TAG" || true
# }
# trap cleanup_tag EXIT

set -x

cd "$(dirname "${BASH_SOURCE[0]}")/../../wasm"

# ensure we make the `./build` directory outside of the container, so
# it's owned by whoever's running this script
mkdir -p ./build/

# TODO switch this over to a -o type=local,dest=... kind of build
#      so we can avoid all these tag/multiple `docker run`s / stateful cmake build dir shenanigans
docker build -f Dockerfile.emscripten . --target=talvos -t "$TAG"

# [ -d ./build/emscripten-docker ] || {
# 	docker run -it --rm -v "$(pwd)":/usr/src -w /usr/src/talvos \
#     "$TAG" -- \
#     cmake -B build/emscripten-docker
# }

# these flags clear the "found" part of `find_packages`'s config mode (and maybe module mode?), so resolution
# is re-run.
# see: https://cmake.org/cmake/help/latest/command/find_package.html
# > Config mode search attempts to locate a configuration file provided by the package to be found.
# > A cache entry called <PackageName>_DIR is created to hold the directory containing the file.
CMAKE_UNSET_FOUND_PACKAGE_CACHE_VARS=(
	-USPIRV-Header_DIR
	-USPIRV-Tools_DIR
)

docker run -it --rm -v "$(pwd)":/usr/src -w /usr/src \
	"$TAG" -- \
	cmake -B build/emscripten-docker  \
	  -DCMAKE_C_FLAGS="-fdebug-compilation-dir=. -ffile-prefix-map=/usr/src=../wasm" \
    -DCMAKE_CXX_FLAGS="-fdebug-compilation-dir=. -ffile-prefix-map=/usr/src=../wasm" \
		"${CMAKE_UNSET_FOUND_PACKAGE_CACHE_VARS[@]}" \
		-DUSE_INSTALLED_SPIRV-Tools:BOOL=TRUE \
		-DCMAKE_FIND_PACKAGE_PREFER_CONFIG:BOOL=TRUE # <-- doesn't seem to work in terms of avoiding rebuilding SPIRV-Tools twice


# TODO something about this?
# ```
# cache:INFO: generating system asset: symbol_lists/1701111f65fe30b91927855eb0451158a36e4416.json... (this will be cached in "/emsdk/upstream/emscripten/cache/symbol_lists/1701111f65fe30b91927855eb0451158a36e4416.json" for subsequent builds)
# ```

docker run -it --rm -v "$(pwd)":/usr/src -w /usr/src \
    "$TAG" -- \
    cmake --build build/emscripten-docker --target talvos-wasm

cp --verbose --update=older build/emscripten-docker/talvos/tools/talvos-cmd/talvos-wasm.* "$1"
