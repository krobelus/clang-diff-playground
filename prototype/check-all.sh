#!/usr/bin/env bash

cd "$(dirname "$0")"/..
set -e

for i in $(seq 1 "$(($(ls tests/java/*.java | wc -l) / 2))")
do
  ./prototype/check.sh $i
done
