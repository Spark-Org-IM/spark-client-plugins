#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

version="$(head -n 1 VERSION | tr -d '[:space:]')"
source_name="$(sed -n 's/^Source:[[:space:]]*//p' debian/control | head -n 1)"
if [ -z "$version" ] || [ -z "$source_name" ]; then
  echo "cannot determine version or Debian source name" >&2
  exit 1
fi

current="$(sed -n '1s/^[^(]* (\([^)]*\)).*/\1/p' debian/changelog 2>/dev/null || true)"

if [ "${1:-}" = "--check" ]; then
  if [ "$current" != "$version" ]; then
    echo "debian/changelog is ${current:-missing}, expected ${version}" >&2
    exit 1
  fi
  exit 0
fi

if [ "$current" != "$version" ]; then
  tmp="$(mktemp)"
  cat >"$tmp" <<CHANGELOG
${source_name} (${version}) unstable; urgency=medium

  * Release ${version}.

 -- Jianyin Li <jianyin@sparkorg.dpdns.org>  $(date -R)
CHANGELOG
  if [ -s debian/changelog ]; then
    echo >>"$tmp"
    cat debian/changelog >>"$tmp"
  fi
  mv "$tmp" debian/changelog
fi
