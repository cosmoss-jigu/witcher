CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -I$(INC_DIR) -Wno-deprecated -DCLWB

DIR = ../../../../third_party/RECIPE/P-CLHT-Aga/src
EXT_DIR = ../../../../third_party/RECIPE/P-CLHT-Aga/external/sspfd
EXT_DIR1 = ../../../../third_party/RECIPE/P-CLHT-Aga/external/ssmem/src

FILES = $(DIR)/clht_lb_res_tx.c

all: bc obj cov

bc : clht_lb_res_tx.bc

clht_lb_res_tx.bc : copy
	$(CC) $(CFLAGS) -emit-llvm clht_lb_res_tx.c

cov: clht_lb_res_tx.cov.o
clht_lb_res_tx.cov.o : copy
	$(CC) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping clht_lb_res_tx.c -o $@
obj : clht_lb_res_tx.o

clht_lb_res_tx.o : copy
	$(CC) $(CFLAGS)  clht_lb_res_tx.c
copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.cpp *.bc *.o *.c
