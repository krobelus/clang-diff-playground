#!/bin/sh

cd "$(dirname "$0")"
cd ..

tidy="clang-tidy -header-filter=clang-diff/"
for src in clang-diff/*.cpp
do
  clang-format -i "$src"
  if [ -n "$1" ]; then
    $tidy "$src" -fix
    # $tidy "$src" -checks='modernize-*' -fix
    # $tidy "$src" -checks='performance-*'
    # $tidy "$src" -checks='readability-*' -fix
    # $tidy "$src" -checks='cppcoreguidelines-*'
    # $tidy "$src" -checks='clang-analyzer-core-*'
    # $tidy "$src" -checks='clang-analyzer-unix-*'
  fi
done

clang-format -i clang-diff/*.h
