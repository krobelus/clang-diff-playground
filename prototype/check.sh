#!/usr/bin/env bash

cd "$(dirname "$0")"/..

if test -n "$1"; then
  files=`echo tests/java/$1{a,b}.java`
else
  files=`echo tests/java/1{a,b}.java`
fi

diff <(./gumtree.sh diff $files | grep -E '^(Match|Insert|Update|Delete)' | sort)\
     <(./prototype/diff.py $files | sort) ||\
     echo "*** failure for $files"
