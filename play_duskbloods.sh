#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
emulator="$script_dir/build/bin/eglantine"
if [[ ! -x "$emulator" ]]; then
    echo "Eglantine executable not found: $emulator" >&2
    exit 1
fi

exec "$emulator" "$@"
