#!/bin/bash

set -euo pipefail

die() {
  echo "Error: $*" >&2
  exit 1
}

require_command() {
  local cmd="$1"
  local message="$2"

  if ! command -v "$cmd" >/dev/null 2>&1; then
    die "$message"
  fi

  if ! "$cmd" --version >/dev/null 2>&1; then
    die "'$cmd' is installed but could not be executed correctly."
  fi
}

require_file() {
  local path="$1"
  local message="$2"

  if [[ ! -f "$path" ]]; then
    die "$message"
  fi
}

version_gte() {
  local actual="$1"
  local required="$2"

  [[ "$(printf '%s\n%s\n' "$required" "$actual" | sort -V | head -n1)" == "$required" ]]
}

CDIR="$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$CDIR"

# glibc: dynamically linked against the system glibc
# musl: fully static, built with zig c++ against musl
VARIANT="${1:-glibc}"

ARCH=$(uname -m)
echo "ARCH: $ARCH"
OUT_ARCH=amd64
if [[ "$ARCH" == "aarch64" ]]; then
  OUT_ARCH=arm64
fi

case "$VARIANT" in
  glibc)
    CONFIGURE_PRESET=ninja-multi-vcpkg
    BUILD_PRESET=ninja-vcpkg-release
    ;;
  musl)
    if ! command -v zig >/dev/null 2>&1 || ! zig version >/dev/null 2>&1; then
      die "zig is required for the static musl build. Install zig and retry."
    fi
    CONFIGURE_PRESET=ninja-multi-vcpkg-zig-static
    BUILD_PRESET=ninja-vcpkg-release-zig-static
    if [[ "$OUT_ARCH" == "arm64" ]]; then
      CONFIGURE_PRESET+=-arm64
      BUILD_PRESET+=-arm64
    fi
    ;;
  *)
    die "Unknown build variant '$VARIANT'. Use 'glibc' or 'musl'."
    ;;
esac

require_command pnpm "pnpm is required to build the web UI. Install pnpm and retry."
require_command cmake "cmake is required to configure and build hs5. Install CMake 3.21 or newer and retry."
require_command xz "xz is required to package the release archive. Install xz-utils and retry."

CMAKE_VERSION="$(cmake --version | head -n1 | awk '{print $3}')"
if ! version_gte "$CMAKE_VERSION" "3.21.0"; then
  die "cmake $CMAKE_VERSION is too old. Install CMake 3.21 or newer and retry."
fi

PYTHON_BIN=""
if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN=python3
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN=python
else
  die "Python is required to run wwwgen. Install python3 and retry."
fi

if ! "$PYTHON_BIN" --version >/dev/null 2>&1; then
  die "'$PYTHON_BIN' is installed but could not be executed correctly."
fi

require_file "$CDIR/www/package.json" "Missing frontend package manifest at $CDIR/www/package.json."
require_file "$CDIR/www/pnpm-lock.yaml" "Missing pnpm lockfile at $CDIR/www/pnpm-lock.yaml."
require_file "$CDIR/wwwgen/wwwgen.py" "Missing site generator script at $CDIR/wwwgen/wwwgen.py."
require_file "$CDIR/CMakePresets.json" "Missing CMakePresets.json in the repository root."
require_file "$CDIR/vcpkg/scripts/buildsystems/vcpkg.cmake" "Missing repo-local vcpkg checkout at $CDIR/vcpkg. Initialize submodules and retry."

cd www
pnpm install --frozen-lockfile
pnpm run build

cd ../wwwgen
"$PYTHON_BIN" wwwgen.py ../www/dist not-empty

cd ..
cmake --preset "$CONFIGURE_PRESET"
cmake --build --preset "$BUILD_PRESET"

xz -z -c "./builds/$CONFIGURE_PRESET/Release/hs5" > "./hs5-$OUT_ARCH-$VARIANT.xz"
