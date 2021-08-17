CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -emit-llvm -I$(INC_DIR) -Wno-deprecated

DIR = ../../../../third_party/agamotto
FILES = $(DIR)/rbtree_map_buggy.c

all: bc obj

bc : rbtree_map_buggy.bc

rbtree_map_buggy.bc : copy
	$(CC) $(CFLAGS) rbtree_map_buggy.c -o rbtree_map_buggy.bc

obj : copy
	$(CC) -g -c -I$(INC_DIR) *.c

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.c *.bc *.o
