#!/bin/bash

CURR_DIR=$WITCHER_HOME/benchmark/pmdk-1.8-deps
LIBPMEM_DIR=$WITCHER_HOME/pmdk-1.8/src/libpmem
LIBPMEMOBJ_DIR=$WITCHER_HOME/pmdk-1.8/src/libpmemobj
INCLUDE_DIR=$WITCHER_HOME/pmdk-1.8/src/include

cd $LIBPMEM_DIR
make -j16
sudo make install NDCTL_ENABLE=n prefix=/usr

cd $LIBPMEMOBJ_DIR
make -j16
sudo make install NDCTL_ENABLE=n prefix=/usr

cd $INCLUDE_DIR
sudo cp -r libpmemblk.h libpmem.h libpmemlog.h libpmemobj.h libpmempool.h libpmemobj /usr/include/

cd $CURR_DIR
cp $LIBPMEMOBJ_DIR/*.bc .
