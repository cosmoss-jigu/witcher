CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -emit-llvm -I$(INC_DIR) -Wno-deprecated

DIR = ../../../third_party/FAST_FAIR/single/src
FILES = $(DIR)/pmdk.c

all: bc obj cov

bc : pmdk.bc

pmdk.bc : copy
	$(CXX) $(CFLAGS) pmdk.c -o pmdk.bc

cov: pmdk.cov.o

pmdk.cov.o: copy
	$(CXX) -g -c -fprofile-instr-generate -fcoverage-mapping -I$(INC_DIR) pmdk.c -o pmdk.cov.o

obj : copy
	$(CXX) -g -c -I$(INC_DIR) *.c

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.c *.bc *.o
