#!/bin/bash

set -e


CDIR="$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$CDIR"

if [ -e ./vcpkg/buildtrees ]
then
    echo "Cleaning vcpkg buildtrees"
    rm -Rf ./vcpkg/buildtrees/*
fi