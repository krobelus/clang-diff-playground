#!/bin/sh

cd "$(dirname "$0")"
java -jar ./apted/build/libs/apted.jar "$@"
