#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 [01|02|03|04|month01|month02|month03|month04|month05|month06|monthXX/weekYY|all] (XX=01..06, YY=01..04)"
}

if [[ $# -eq 1 && ( $1 == --help || $1 == -h ) ]]; then
    usage
    exit 0
fi
if [[ $# -gt 1 ]]; then
    usage >&2
    exit 2
fi

case "${1:-all}" in
    01|02|03|04) course="month01/week$1" ;;
    all|month01|month02|month03|month04|month05|month06|month0[123456]/week0[1234]) course="${1:-all}" ;;
    *) usage >&2; exit 2 ;;
esac

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
case "$course" in
    all) weeks=$(cd "$script_dir" && echo month*/week0[1-4]) ;;
    month0[123456]/week0[1234]) weeks="$course" ;;
    month0*) weeks=$(cd "$script_dir" && echo "$course"/week0[1-4]) ;;
    *) weeks="$course" ;;
esac
for week in $weeks; do
    python3 "$script_dir/$week/builder.py" build
done
