CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -emit-llvm -I$(INC_DIR) -Wno-deprecated

DIR = ../../../../third_party/pmtest/nvml/src/examples/libpmemobj/tree_map
FILES = $(DIR)/btree_map.c

all: bc obj

bc : btree_map.bc

btree_map.bc : copy
	$(CC) $(CFLAGS) btree_map.c -o btree_map.bc

obj : copy
	$(CC) -g -c -I$(INC_DIR) *.c

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.c *.bc *.o
