#!/usr/bin/env bash

cd "$(dirname "$0")"

if test -n "$1"; then
  files=`echo examples/$1{a,b}.java`
else
  files=`echo examples/1{a,b}.java`
fi

diff <(./gumtree.sh diff $files | grep '^Match' | sort)\
     <(./diff.py $files | sort) ||\
     echo "*** failure for $files"
