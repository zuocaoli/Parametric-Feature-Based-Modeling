#!/usr/bin/env bash
set -euo pipefail

if (( $# != 0 )); then
    printf '%s\n' '本课程不接受命令行参数，请在 main.cpp 中修改示例参数。' >&2
    exit 2
fi

course_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$course_dir"

cmake --preset default
cmake --build --preset default

printf '\n\n====================================================\n\n\n'

exec "$course_dir/build/demo"
