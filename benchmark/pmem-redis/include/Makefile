CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++ 
DIR = ../../../third_party/pmem-redis/src
FILES = $(DIR)/*.h
DEPS = ../../../third_party/pmem-redis/deps
#DEPS_TARGET = pmdk
DEPS_TARGET = hiredis linenoise lua jemallocat pmdk memkind jemalloc
.PHONY: all copy clean

all: copy

copy:
	@ cp $(FILES) .
	@ cp -r $(DEPS) ../
	@ cd ../deps && $(MAKE) $(DEPS_TARGET) CC=$(CC)  CXX=$(CXX)

clean:
	@ rm -f *.h
	@ rm -rf ../deps
