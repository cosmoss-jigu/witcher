#!/bin/bash

CC=${LLVM9_BIN}/clang
CXX=${LLVM9_BIN}/clang++
OPT=${LLVM9_BIN}/opt
LLC=${LLVM9_BIN}/llc
LLVM_LINK=${LLVM9_BIN}/llvm-link
LLVM_AS=${LLVM9_BIN}/llvm-as


rm -rf ${WITCHER_HOME}/benchmark/memcached-pmem/lib
cp -r ${WITCHER_HOME}/third_party/memcached-pmem/ ${WITCHER_HOME}/benchmark/memcached-pmem/lib
cd ${WITCHER_HOME}/benchmark/memcached-pmem/lib
autoreconf -f -i
./configure --enable-pslab
make -j32

CFLAGS="-DHAVE_CONFIG_H -I. -DNDEBUG -g -O0 -pthread -pthread -Wall -Wno-error -pedantic -MD -MP -c"
BC_FILES=*.bc
# Generate memcached.bc and .bc files for memcached.c and other bc files for the src files.
${CC} ${CFLAGS} -MT memcached-memcached.ll -MF .deps/memcached-memcached.Tpo -emit-llvm -S -o memcached-memcached.ll `test -f 'memcached.c' || echo './'`memcached.c
mv -f .deps/memcached-memcached.Tpo .deps/memcached-memcached.Po
${LLVM_AS} memcached-memcached.ll -o memcached-memcached.bc
${CC} ${CFLAGS} -MT memcached-assoc.ll -MF .deps/memcached-assoc.Tpo -emit-llvm -S -o memcached-assoc.ll `test -f 'assoc.c' || echo './'`assoc.c
mv -f .deps/memcached-assoc.Tpo .deps/memcached-assoc.Po
${LLVM_AS} memcached-assoc.ll -o memcached-assoc.bc
${CC} ${CFLAGS} -MT memcached-bipbuffer.ll -MF .deps/memcached-bipbuffer.Tpo -emit-llvm -S -o memcached-bipbuffer.ll `test -f 'bipbuffer.c' || echo './'`bipbuffer.c
mv -f .deps/memcached-bipbuffer.Tpo .deps/memcached-bipbuffer.Po
${LLVM_AS} memcached-bipbuffer.ll -o memcached-bipbuffer.bc
${CC} ${CFLAGS} -MT memcached-cache.ll -MF .deps/memcached-cache.Tpo -emit-llvm -S -o memcached-cache.ll `test -f 'cache.c' || echo './'`cache.c
mv -f .deps/memcached-cache.Tpo .deps/memcached-cache.Po
${LLVM_AS} memcached-cache.ll -o memcached-cache.bc
${CC} ${CFLAGS} -MT memcached-crawler.ll -MF .deps/memcached-crawler.Tpo -emit-llvm -S -o memcached-crawler.ll `test -f 'crawler.c' || echo './'`crawler.c
mv -f .deps/memcached-crawler.Tpo .deps/memcached-crawler.Po
${LLVM_AS} memcached-crawler.ll -o memcached-crawler.bc
${CC} ${CFLAGS} -MT memcached-daemon.ll -MF .deps/memcached-daemon.Tpo -emit-llvm -S -o memcached-daemon.ll `test -f 'daemon.c' || echo './'`daemon.c
mv -f .deps/memcached-daemon.Tpo .deps/memcached-daemon.Po
${LLVM_AS} memcached-daemon.ll -o memcached-daemon.bc
${CC} ${CFLAGS} -MT memcached-hash.ll -MF .deps/memcached-hash.Tpo -emit-llvm -S -o memcached-hash.ll `test -f 'hash.c' || echo './'`hash.c
mv -f .deps/memcached-hash.Tpo .deps/memcached-hash.Po
${LLVM_AS} memcached-hash.ll -o memcached-hash.bc
${CC} ${CFLAGS} -MT memcached-items.ll -MF .deps/memcached-items.Tpo -emit-llvm -S -o memcached-items.ll `test -f 'items.c' || echo './'`items.c
mv -f .deps/memcached-items.Tpo .deps/memcached-items.Po
${LLVM_AS} memcached-items.ll -o memcached-items.bc
${CC} ${CFLAGS} -MT memcached-itoa_ljust.ll -MF .deps/memcached-itoa_ljust.Tpo -emit-llvm -S -o memcached-itoa_ljust.ll `test -f 'itoa_ljust.c' || echo './'`itoa_ljust.c
mv -f .deps/memcached-itoa_ljust.Tpo .deps/memcached-itoa_ljust.Po
${LLVM_AS} memcached-itoa_ljust.ll -o memcached-itoa_ljust.bc
${CC} ${CFLAGS} -MT memcached-jenkins_hash.ll -MF .deps/memcached-jenkins_hash.Tpo -emit-llvm -S -o memcached-jenkins_hash.ll `test -f 'jenkins_hash.c' || echo './'`jenkins_hash.c
mv -f .deps/memcached-jenkins_hash.Tpo .deps/memcached-jenkins_hash.Po
${LLVM_AS} memcached-jenkins_hash.ll -o memcached-jenkins_hash.bc
${CC} ${CFLAGS} -MT memcached-logger.ll -MF .deps/memcached-logger.Tpo -emit-llvm -S -o memcached-logger.ll `test -f 'logger.c' || echo './'`logger.c
mv -f .deps/memcached-logger.Tpo .deps/memcached-logger.Po
${LLVM_AS} memcached-logger.ll -o memcached-logger.bc
${CC} ${CFLAGS} -MT memcached-murmur3_hash.ll -MF .deps/memcached-murmur3_hash.Tpo -emit-llvm -S -o memcached-murmur3_hash.ll `test -f 'murmur3_hash.c' || echo './'`murmur3_hash.c
mv -f .deps/memcached-murmur3_hash.Tpo .deps/memcached-murmur3_hash.Po
${LLVM_AS} memcached-murmur3_hash.ll -o memcached-murmur3_hash.bc
${CC} ${CFLAGS} -MT memcached-pslab.ll -MF .deps/memcached-pslab.Tpo -emit-llvm -S -o memcached-pslab.ll `test -f 'pslab.c' || echo './'`pslab.c
mv -f .deps/memcached-pslab.Tpo .deps/memcached-pslab.Po
${LLVM_AS} memcached-pslab.ll -o memcached-pslab.bc
${CC} ${CFLAGS} -MT memcached-slab_automove.ll -MF .deps/memcached-slab_automove.Tpo -emit-llvm -S -o memcached-slab_automove.ll `test -f 'slab_automove.c' || echo './'`slab_automove.c
mv -f .deps/memcached-slab_automove.Tpo .deps/memcached-slab_automove.Po
${LLVM_AS} memcached-slab_automove.ll -o memcached-slab_automove.bc
${CC} ${CFLAGS} -MT memcached-slabs.ll -MF .deps/memcached-slabs.Tpo -emit-llvm -S -o memcached-slabs.ll `test -f 'slabs.c' || echo './'`slabs.c
mv -f .deps/memcached-slabs.Tpo .deps/memcached-slabs.Po
${LLVM_AS} memcached-slabs.ll -o memcached-slabs.bc
${CC} ${CFLAGS} -MT memcached-stats.ll -MF .deps/memcached-stats.Tpo -emit-llvm -S -o memcached-stats.ll `test -f 'stats.c' || echo './'`stats.c
mv -f .deps/memcached-stats.Tpo .deps/memcached-stats.Po
${LLVM_AS} memcached-stats.ll -o memcached-stats.bc
${CC} ${CFLAGS} -MT memcached-thread.ll -MF .deps/memcached-thread.Tpo -emit-llvm -S -o memcached-thread.ll `test -f 'thread.c' || echo './'`thread.c
mv -f .deps/memcached-thread.Tpo .deps/memcached-thread.Po
${LLVM_AS} memcached-thread.ll -o memcached-thread.bc
${CC} ${CFLAGS} -MT memcached-util.ll -MF .deps/memcached-util.Tpo -emit-llvm -S -o memcached-util.ll `test -f 'util.c' || echo './'`util.c
mv -f .deps/memcached-util.Tpo .deps/memcached-util.Po
${LLVM_AS} memcached-util.ll -o memcached-util.bc

#Merge all bc into one bc
${LLVM_LINK} ${BC_FILES} ${WITCHER_HOME}/benchmark/pmdk-1.8-deps/*.bc -o memcached.all.bc

# cp the result to the main dir
cp memcached ../main/main.exe
cp memcached.all.bc ../main/main.all.bc
