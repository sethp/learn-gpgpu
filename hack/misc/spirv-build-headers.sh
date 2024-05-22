#!/bin/bash

set -euo pipefail

usage() {
	echo >&2 "Usage: $0 [-w]"
}

while [[ $# -gt 0 ]]; do
case "$1" in
	-w|--watch)
		MODE=watch
		shift;
	;;
	-h|--help)
		usage
		exit
	;;
	**)
		echo >&2 "Unrecognized flag: $1"
		usage
		exit 2
	;;
esac
done

# the hope for this block was a easy & simple way to turn any script "watch-able"
# so far that's run aground on the details:
# - where does the script live?
#		if we make it convenient to express paths (with `cd`), then how does entr find the script?
#   what all needs to change if this script's location moves?
# - who restarts `entr` when files are added to directories?
[ "${MODE:-}" = watch ] && {
	PREFIX="$(dirname "${BASH_SOURCE[0]}")/../.."

	IFS=$'\n'
	# mommas, don't let your babies put newlines in their filenames
	# shellcheck disable=SC2207
	FILES=(
		"$0"
		"$PREFIX"/wasm/SPIRV-Headers/include/spirv/unified1/spirv.core.grammar.json
		$(find "$PREFIX"/wasm/SPIRV-Headers/tools -name build -prune -o -type f -print)
	)
	# also don't export IFS; but nobody does that either (right?)
	<<<"${FILES[*]}" exec entr "$0"
	# unreachable
	exit 255
}

cd "$(dirname "${BASH_SOURCE[0]}")/../../wasm/SPIRV-Headers"


# cf. https://github.com/KhronosGroup/SPIRV-Headers/tree/main?tab=readme-ov-file#generating-headers-from-the-json-grammar-for-the-spir-v-core-instruction-set

command -v dos2unix >/dev/null || {
	echo >&2 "ERROR missing tool: please install \`dos2unix\`"
	exit 2
}

mkdir -p tools/buildHeaders/build
cmake -S tools/buildHeaders -B tools/buildHeaders/build
cmake --build tools/buildHeaders/build --target install

cd ./tools/buildHeaders/
bin/makeHeaders
