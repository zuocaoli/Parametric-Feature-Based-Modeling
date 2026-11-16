#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
buildDir="$scriptDir/build"

cmakeArgs=(
    -S "$scriptDir"
    -B "$buildDir"
    -G Ninja
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

if [[ -f "$buildDir/CMakeCache.txt" ]] &&
   ! grep -Fqx "CMAKE_HOME_DIRECTORY:INTERNAL=$scriptDir" "$buildDir/CMakeCache.txt"; then
    cmake --fresh "${cmakeArgs[@]}"
else
    cmake "${cmakeArgs[@]}"
fi

cmake --build "$buildDir" --parallel
