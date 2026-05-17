#!/bin/zsh
set -euo pipefail

repo_root="${0:A:h:h}"
app_path="$repo_root/build/MySynth_artefacts/Debug/StageFM.app"

if [[ ! -d "$app_path" ]]; then
    echo "Debug app not found: $app_path" >&2
    echo "Build it first with: cmake --build build --config Debug --target MySynth -j\$(sysctl -n hw.logicalcpu)" >&2
    exit 1
fi

/usr/bin/pkill -x StageFM >/dev/null 2>&1 || true
/usr/bin/open -n "$app_path"