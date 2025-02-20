#!/bin/bash

set -ex

CDIR="$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$CDIR"

cd www

pnpm install --frozen-lockfile

pnpm run build

cd ../wwwgen

python wwwgen.py ../www/dist not-empty

cd ..

cmake --preset ninja-multi-vcpkg

cmake --build --preset ninja-vcpkg-release

OUT_FN=hs5.xz
ARCH=$(uname -m)
echo "ARCH: $ARCH"
if [[ "$ARCH" == "aarch64" ]]; then
  OUT_FN=hs5-arm64.xz
fi

xz -z -c ./builds/ninja-multi-vcpkg/Release/hs5 > ./$OUT_FN

