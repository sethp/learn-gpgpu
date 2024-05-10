#!/bin/bash

set -euo pipefail

usage() {
    echo >&2 "usage: $0"
    echo >&2 ""
    echo >&2 "updates the SPIRV-Headers submodule based on the SPIRV-Tools' \`DEPS\`."
    echo >&2 "also changes the expected branch to BRANCH_NAME "
}

[[ $# -gt 0 || "${1:-}" == '-'* ]] && {
	usage
	exit 2
}

cd "$(dirname "${BASH_SOURCE[0]}")/../.."


# In order to check out the right SPIRV-Headers for the release, we have to read the DEPS file
IFS=@ read REPO REF < <(cd wasm/SPIRV-Tools; <<-'PYTHON' python3 -u
Var = lambda key: vars[key]
DEPS = open("DEPS").read()
exec(DEPS)
# prints something like:
#   `https://github.com/KhronosGroup/SPIRV-Headers.git@85a1ed200d50660786c1a88d9166e871123cce39`
print(deps["external/spirv-headers"])
PYTHON
)

echo "Read from SPIRV-Tools' \`DEPS\`: $REPO $REF"

set -x

git submodule set-url wasm/SPIRV-Headers "$REPO"
git -C wasm/SPIRV-Headers fetch

STASH_COMMIT=$(git -C wasm/SPIRV-Headers stash create --all)

git -C wasm/SPIRV-Headers reset --hard "$REF" || {
	if [ -n "$STASH_COMMIT" ] ; then
		echo >&2 "couldn't reset; saving temporary commit $STASH_COMMIT to stash"
		git -C wasm/SPIRV-Headers stash store "$STASH_COMMIT"
	fi
	exit 1
}

 [ -n "$STASH_COMMIT" ] && \
 	git -C wasm/SPIRV-Headers stash apply --index "$STASH_COMMIT"
