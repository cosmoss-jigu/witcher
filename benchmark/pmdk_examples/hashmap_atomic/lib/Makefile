CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -emit-llvm -I$(INC_DIR) -Wno-deprecated

DIR = ../../../../third_party/pmtest/nvml/src/examples/libpmemobj/hashmap
FILES = $(DIR)/hashmap_atomic.c

all: bc obj

bc : hashmap_atomic.bc

hashmap_atomic.bc : copy
	$(CC) $(CFLAGS) hashmap_atomic.c -o hashmap_atomic.bc

obj : copy
	$(CC) -g -c -I$(INC_DIR) *.c

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.c *.bc *.o
