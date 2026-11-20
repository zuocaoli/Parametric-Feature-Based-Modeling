#!/usr/bin/env bash
set -euo pipefail

course_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$course_dir"

quiet() {
    local output status=0
    output=$("$@" 2>&1) || status=$?
    if (( status != 0 )); then
        printf '%s\n' "$output" >&2
        exit "$status"
    fi
}

quiet cmake --preset default
quiet cmake --build --preset default
exec "$course_dir/build/demo" "$@"
