#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <01|02|03|04|monthXX/weekYY> (XX=01..06, YY=01..04)"
}

if [[ $# -eq 1 && ( $1 == --help || $1 == -h ) ]]; then
    usage
    exit 0
fi
if [[ $# -ne 1 ]]; then
    usage >&2
    exit 2
fi

case "$1" in
    01|02|03|04) course="month01/week$1" ;;
    month0[123456]/week0[1234]) course="$1" ;;
    *) usage >&2; exit 2 ;;
esac

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
executable="$script_dir/$course/build/demo"
if [[ ! -x "$executable" ]]; then
    echo "未找到可执行程序：$executable。请先执行：\"$script_dir/build.sh\" $1" >&2
    exit 1
fi
exec "$executable"
