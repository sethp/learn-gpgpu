#!/bin/bash

# see: https://stackoverflow.com/questions/2285403/how-to-make-shell-scripts-robust-to-source-being-changed-as-they-run/
{
set -euo pipefail

usage() {
    echo >&2 "usage: $0 [options]"
    echo >&2 ""
    echo >&2 "builds wasm artifacts"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    # options
    -h|--help)
      usage
      exit
    ;;
    -v|--verbose)
			declare VERBOSE='on'
    ;;
    -w|--watch)
			declare WATCH_MODE='on'
			# TODO failing faster if `entr` doesn't exist is nicer
			# TODO with maybe a way to say `--no-watch` to not hide the dep behind a flag, too? (but still allow e.g. CI)
			#      or pull apart into two scripts; "watch" and "build"
		;;

    # default
    *)
      echo >&2 "unrecognized argument: '$1'"
      usage
      exit 2
    ;;
  esac
  shift
done

[[ $# -eq 0 ]] || {
	usage
	exit 2
}

[[ -v VERBOSE ]] && set -x;

cd "$(dirname "${BASH_SOURCE[0]}")/.."

./wasm/talvos/hack/build-wasm.sh "$PWD/wasm"

[[ -v WATCH_MODE ]] || exit 0;

watch() {
	# via https://superuser.com/a/665208
	while (
		[[ -v VERBOSE ]] && echo >&2 "Watching files....";

		(
			(
				echo wasm/talvos/Dockerfile.emscripten
				find wasm/talvos -name 'build' -prune -o \( -name 'CMakeLists.txt' -o -name '*.cmake' \) -print
				find wasm/talvos -name 'build' -prune -o -name '*.h' -print
				find wasm/talvos -name '*.c' -o -name '*.cpp'
			) \
		| tee ${VERBOSE+/dev/stderr} \
		) \
		| entr -c -d -p "$0" ${VERBOSE+-v} # no --watch

		# via `man entr`
		#  2       A file was added to a directory and the directory watch option was specified
		[[ "${PIPESTATUS[1]}" -eq 2 ]]
	); do
		[[ -v VERBOSE ]] && echo >&2 "Restarting watcher";
	done

	# important to keep bash from executing "off the end" of the file if it's changed
	exit
}

watch
}
