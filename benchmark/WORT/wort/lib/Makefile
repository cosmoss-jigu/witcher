CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -I$(INC_DIR) -Wno-deprecated

DIR = ../../../../third_party/WORT/src/wort
FILES = $(DIR)/pmdk.c $(DIR)/wort.c

all: bc obj cov

bc : wort.bc pmdk.bc
wort.bc : copy
	$(CC) $(CFLAGS) -emit-llvm  wort.c
pmdk.bc : copy
	$(CC) $(CFLAGS) -emit-llvm  pmdk.c

cov: wort.cov.o pmdk.cov.o
wort.cov.o : copy
	$(CC) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping wort.c -o wort.cov.o
pmdk.cov.o : copy
	$(CC) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping pmdk.c -o pmdk.cov.o

obj : wort.o pmdk.o
wort.o : copy
	$(CC) $(CFLAGS) wort.c
pmdk.o : copy
	$(CC) $(CFLAGS) pmdk.c

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.cpp *.bc *.o
