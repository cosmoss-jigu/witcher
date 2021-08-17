CC = $(LLVM9_BIN)/clang
DIR = ../../../third_party/redis-3.2-nvml/src
FILES = $(DIR)/*.h
DEPS = ../../../third_party/redis-3.2-nvml/deps
DEPS_TARGET = geohash-int hiredis linenoise lua
.PHONY: all copy clean

all: copy

copy:
	@ cp $(FILES) .
	@ cp -r $(DEPS) ../
	@ cd ../deps && $(MAKE) $(DEPS_TARGET) CC=$(CC)

clean:
	@ rm -f *.h
	@ rm -rf ../deps
