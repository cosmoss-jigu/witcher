CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++

INC_DIR = ../
CFLAGS = -g -O0 -c -emit-llvm -I$(INC_DIR) -Wno-deprecated

DIR = ../../../third_party/CCEH/src
FILES = $(DIR)/hash.h $(DIR)/CCEH.h $(DIR)/CCEH_MSB.cpp $(DIR)/pmdk.h $(DIR)/pmdk.c

all: bc obj cov

bc: CCEH_MSB.bc pmdk.bc

CCEH_MSB.bc : copy
	$(CXX) -std=c++11 $(CFLAGS) CCEH_MSB.cpp -o CCEH_MSB.bc -DINPLACE

pmdk.bc : copy
	$(CXX) $(CFLAGS) pmdk.c -o pmdk.bc

cov: CCEH_MSB.cov.o pmdk.cov.o

pmdk.cov.o: copy
	$(CXX) -g -c -fprofile-instr-generate -fcoverage-mapping pmdk.c -o pmdk.cov.o

CCEH_MSB.cov.o : copy
	$(CXX) -std=c++11 -g -c -fprofile-instr-generate -fcoverage-mapping -I$(INC_DIR) CCEH_MSB.cpp -o CCEH_MSB.cov.o -DINPLACE

obj : copy
	$(CXX) -g -c pmdk.c
	$(CXX) -std=c++11 -g -c -I$(INC_DIR) CCEH_MSB.cpp -DINPLACE

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.h *.c *.cpp *.bc *.o
