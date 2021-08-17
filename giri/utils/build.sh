#!/bin/bash

mkdir build
cd build

#../configure --with-llvmsrc=$LLVM_HOME \
#			 --with-llvmobj=$LLVM_HOME/build \
#			 --enable-optimized \
#			 --disable-debug-symbols \
#			 --disable-docs \
#			 --disable-terminfo \
#			 --enable-targets=host-only

../configure --with-llvmsrc=$LLVM_HOME \
       --with-llvmobj=$LLVM_HOME/build \
       --enable-optimized \
       --enable-debug-symbols \
       --enable-debug-runtime \
       --disable-docs \
       --disable-terminfo \
       --enable-targets=host-only
make
cd ..
make -C test
