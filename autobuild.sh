#!/bin/bash

set -e

rm -rf `pwd`/build
mkdir build

cd `pwd`/build &&
    cmake .. &&
    make -j4