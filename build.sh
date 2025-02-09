#!/bin/bash

set -ex

CDIR="$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$CDIR"

cd www

pnpm install --frozen-lockfile

pnpm run build

cd ../wwwgen

python wwwgen.py ../www/dist not-empty

cmake --preset ninja-multi-vcpkg

cmake --build --preset ninja-vcpkg-release



