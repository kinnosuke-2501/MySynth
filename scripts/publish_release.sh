#!/bin/zsh
# OUTWARD-FACING: publishes a public GitHub Release + a public GitHub Pages
# site. Run only when you intend to make this build downloadable by others.
#
#   ./scripts/package_release.sh        # build the zip first
#   ./scripts/publish_release.sh        # then publish
#
# Requires: gh (already logged in). No Apple Developer account needed.
set -euo pipefail

repo_root="${0:A:h:h}"
cd "$repo_root"

ver="$(sed -n 's/^project(MySynth VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)"
ver="${ver:-1.0.0}"
zip="dist/StageFM-${ver}-macOS.zip"
tag="v${ver}"
slug="$(gh repo view --json nameWithOwner -q .nameWithOwner)"
owner="${slug%%/*}"; repo="${slug##*/}"

[[ -f "$zip" ]] || { echo "Missing $zip — run scripts/package_release.sh first" >&2; exit 1; }

notes="StageFM ${ver} — macOS standalone (Universal, macOS 11+).

Unsigned: first launch is right-click → Open (see the install page).
https://${owner}.github.io/${repo}/"

echo "==> GitHub Release $tag"
if gh release view "$tag" >/dev/null 2>&1; then
    gh release upload "$tag" "$zip" --clobber
else
    gh release create "$tag" "$zip" --title "StageFM ${ver}" --notes "$notes"
fi

echo "==> Deploy site/ to gh-pages"
tmp="$(mktemp -d)"
cp -R site/. "$tmp"/
(
    cd "$tmp"
    git init -q
    git checkout -q -b gh-pages
    git add -A
    git -c user.email="deploy@local" -c user.name="deploy" commit -qm "site ${ver}"
    git push -q -f "https://github.com/${slug}.git" gh-pages
)
rm -rf "$tmp"

echo "==> Enable GitHub Pages (gh-pages branch, idempotent)"
gh api -X POST "repos/${slug}/pages" -f "source[branch]=gh-pages" -f "source[path]=/" >/dev/null 2>&1 \
  || gh api -X PUT "repos/${slug}/pages" -f "source[branch]=gh-pages" -f "source[path]=/" >/dev/null 2>&1 \
  || true

echo
echo "Release : https://github.com/${slug}/releases/latest"
echo "Web page: https://${owner}.github.io/${repo}/   (Pages build takes ~1 min)"
