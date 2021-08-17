#!/bin/bash

cd $WITCHER_HOME/benchmark/pmdk-1.8-deps
./run.sh

cd $WITCHER_HOME/giri/build-llvm9
make -j16
