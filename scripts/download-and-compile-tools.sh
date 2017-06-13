#!/bin/sh

set -e

git clone https://github.com/DatabaseGroup/apted
cd apted
gradle build
cd ..

git clone https://github.com/GumTreeDiff/gumtree
cd gumtree
./gradlew build -x test
unzip dist/build/distributions/gumtree-*.zip -d ..
cd ..
