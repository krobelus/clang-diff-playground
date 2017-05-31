#!/usr/bin/env bash

cd "$(dirname "$0")"
set -e

for i in $(seq 1 "$(($(ls examples/*.java | wc -l) / 2))")
do
  ./check.sh $i
done
