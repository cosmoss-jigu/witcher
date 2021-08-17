CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -I$(INC_DIR) -Wno-deprecated

DIR = ../../../../third_party/WORT/src/woart
FILES = $(DIR)/pmdk.c $(DIR)/woart.c

all: bc obj cov

bc : woart.bc pmdk.bc
woart.bc : copy
	$(CC) $(CFLAGS) -emit-llvm  woart.c
pmdk.bc : copy
	$(CC) $(CFLAGS) -emit-llvm  pmdk.c

cov: woart.cov.o pmdk.cov.o
woart.cov.o : copy
	$(CC) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping woart.c -o woart.cov.o
pmdk.cov.o : copy
	$(CC) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping pmdk.c -o pmdk.cov.o

obj : woart.o pmdk.o
woart.o : copy
	$(CC) $(CFLAGS) woart.c
pmdk.o : copy
	$(CC) $(CFLAGS) pmdk.c

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.cpp *.bc *.o
