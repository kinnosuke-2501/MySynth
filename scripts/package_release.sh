#!/bin/zsh
# Build a Release, Universal (arm64 + x86_64) .app and zip it for sharing.
# Output: dist/StageFM-<version>-macOS.zip
#
#   ./scripts/package_release.sh
#
# The zip is unsigned: on the friend's Mac, first launch is right-click → Open
# (see site/index.html / README "配布"). No Apple Developer account needed.
set -euo pipefail

repo_root="${0:A:h:h}"
cd "$repo_root"

build_dir="build-release"
config="Release"
app_name="StageFM"
version="$(sed -n 's/^project(MySynth VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)"
version="${version:-1.0.0}"

echo "==> Configuring ($config, Universal arm64+x86_64)"
cmake -B "$build_dir" -DCMAKE_BUILD_TYPE="$config" -Wno-dev >/dev/null

echo "==> Building"
cmake --build "$build_dir" --config "$config" -j"$(sysctl -n hw.logicalcpu)" >/dev/null

app_path="$build_dir/MySynth_artefacts/$config/$app_name.app"
[[ -d "$app_path" ]] || { echo "ERROR: $app_path not found" >&2; exit 1; }

echo "==> Architectures:"
lipo -archs "$app_path/Contents/MacOS/$app_name"

mkdir -p dist
zip_path="dist/${app_name}-${version}-macOS.zip"
rm -f "$zip_path"
# ditto preserves the bundle structure / macOS metadata correctly
ditto -c -k --keepParent "$app_path" "$zip_path"

echo "==> Done: $zip_path ($(du -h "$zip_path" | cut -f1))"
