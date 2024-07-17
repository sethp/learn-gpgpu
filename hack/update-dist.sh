#!/bin/bash

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

# TODO should probably also check for untracked files at some point
( git diff --exit-code >/dev/null && git diff --cached --exit-code >/dev/null ) || {
	echo >&2 "working tree has changes, refusing to update dist"
	echo >&2 "(either commit or stash them)"
	exit 1
}



echo -n "from "
pwd

set -x

git rm -rf dist/
docker build -f Dockerfile.build . -o type=local,dest=dist
git add dist
