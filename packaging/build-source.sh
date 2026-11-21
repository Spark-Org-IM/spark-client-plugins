#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

version="${1:-$(head -n 1 "$root/VERSION" | tr -d '[:space:]')}"
out="${2:-$root/build/source}"
ref="${3:-HEAD}"
source_name="$(sed -n 's/^Source:[[:space:]]*//p' "$root/debian/control" | head -n 1)"

if [ -z "$version" ] || [ -z "$source_name" ]; then
  echo "cannot determine Debian version or source name" >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

stage="$tmp/${source_name}-${version}"
mkdir -p "$stage"
git archive --format=tar "$ref" | tar -x -C "$stage"

# Never leak debhelper build artifacts from the working tree.
rm -rf "$stage/debian/.debhelper" "$stage/debian/tmp" \
       "$stage/debian/files" "$stage/debian/debhelper-build-stamp" \
       "$stage/debian/sparkapp-plugin-news" \
       "$stage/debian/sparkapp-plugin-forum" \
       "$stage"/debian/*.substvars "$stage"/debian/*.debhelper.log \
       "$stage"/obj-* "$stage"/build
sed -i "1s/^[^(]* ([^)]*)/${source_name} (${version})/" "$stage/debian/changelog"

(
  cd "$stage"
  # -S: source package only; -d: build deps are already present when needed;
  # -nc: the staging tree is clean, so no debhelper clean hooks.
  dpkg-buildpackage -S -us -uc -d -nc
)

rm -rf "$out"
mkdir -p "$out"
mv "$tmp"/"${source_name}"_* "$out"/
ls -lh "$out"
