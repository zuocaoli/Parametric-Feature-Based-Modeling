#!/usr/bin/env bash
set -euo pipefail

course_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$course_dir"

python3 "$course_dir/tools/builder.py" build >/dev/null
python3 "$course_dir/tools/builder.py" configure >/dev/null
python3 "$course_dir/tools/check_navigation.py" >/dev/null
exec "$course_dir/build/demo" "$@"
