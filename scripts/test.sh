#!/bin/sh

set -e

cd "$(dirname "$0")"
cd ..

BUILD=tests/build
rm -rf "$BUILD"
mkdir -p "$BUILD"

tidy="clang-tidy -header-filter=clang-diff/"

die() {
  echo "$@"
  exit 1
}

for src in tests/crafted/*a.cpp
# for src in tests/crafted/1a.cpp
do
  dst="$(echo "$src" | sed 's/a\.cpp/b\.cpp/g')"
  mkdir -p "$BUILD"/"$(dirname "$src")"
  mkdir -p "$BUILD"/"$(dirname "$dst")"
  jsonsrc="$BUILD/$src.json"
  jsondst="$BUILD/$dst.json"
  clang-diff -ast-dump "$src" | json_pp > "$jsonsrc"
  clang-diff -ast-dump "$dst" | json_pp > "$jsondst"

  # clang-diff "$src" "$dst"
  # ./prototype/diff.py "$BUILD"/"$src".json "$BUILD"/"$dst".json


  # clang-diff "$src" "$dst" | sort > "$BUILD/clang.treediff"
  # ./prototype/diff.py "$jsonsrc" "$jsondst" | sort > "$BUILD/proto.treediff"
  clang-diff "$src" "$dst" | grep '^Match' | sort > "$BUILD/clang.treediff"
  ./prototype/diff.py "$jsonsrc" "$jsondst" | grep '^Match' | sort > "$BUILD/proto.treediff"
  diff "$BUILD"/{clang,proto}.treediff || die "*** failure for $src"
done

rm -f "$BUILD"/{clang,proto}.treediff
