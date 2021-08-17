CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -I$(INC_DIR) -Wno-deprecated -DCLWB

DIR = ../../../../third_party/RECIPE/P-Masstree
FILES = $(DIR)/pmdk.cpp

all: bc obj cov

bc : pmdk.bc
pmdk.bc : copy
	$(CXX) $(CFLAGS) -emit-llvm  pmdk.cpp

cov : pmdk.cov.o
pmdk.cov.o : copy
	$(CXX) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping pmdk.cpp -o $@

obj : pmdk.o
pmdk.o : copy
	$(CXX) $(CFLAGS) pmdk.cpp

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.cpp *.bc *.o
