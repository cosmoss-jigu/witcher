CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c  -I$(INC_DIR) -Wno-deprecated -DCLWB -DBWTREE_NODEBUG -DNDEBUG 

DIR = ../../../../third_party/RECIPE/P-BwTree/src
FILES = $(DIR)/pmdk.cpp $(DIR)/bwtree.cpp
all: bc obj cov

bc : bwtree.bc pmdk.bc
bwtree.bc : copy
	$(CXX) $(CFLAGS) -emit-llvm bwtree.cpp
pmdk.bc : copy
	$(CXX) $(CFLAGS) -emit-llvm  pmdk.cpp

cov : bwtree.cov.o pmdk.cov.o
bwtree.cov.o : copy
	$(CXX) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping bwtree.cpp -o $@
pmdk.cov.o : copy
	$(CXX) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping pmdk.cpp -o $@

obj : bwtree.o pmdk.o
bwtree.o : copy
	$(CXX) $(CFLAGS) bwtree.cpp
pmdk.o : copy
	$(CXX) $(CFLAGS) pmdk.cpp
copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.cpp *.bc *.o
