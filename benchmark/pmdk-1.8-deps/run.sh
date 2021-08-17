#!/bin/bash

CURR_DIR=$WITCHER_HOME/benchmark/pmdk-1.8-deps
LIBPMEM_DIR=$WITCHER_HOME/pmdk-1.8/src/libpmem
LIBPMEMOBJ_DIR=$WITCHER_HOME/pmdk-1.8/src/libpmemobj

cd $LIBPMEM_DIR
make -j16
sudo make install prefix=/usr

cd $LIBPMEMOBJ_DIR
make -j16
sudo make install prefix=/usr

cd $CURR_DIR
cp $LIBPMEMOBJ_DIR/*.bc .
