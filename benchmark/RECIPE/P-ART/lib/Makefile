CC = $(LLVM9_BIN)/clang
CXX = $(LLVM9_BIN)/clang++
INC_DIR = ../include
CFLAGS = -g -O0 -c -I$(INC_DIR) -Wno-deprecated -DCLWB

DIR = ../../../../third_party/RECIPE/P-ART
FILES = $(DIR)/Epoche.cpp $(DIR)/N16.cpp $(DIR)/N256.cpp $(DIR)/N48.cpp $(DIR)/N4.cpp $(DIR)/N.cpp $(DIR)/pmdk.cpp $(DIR)/Tree.cpp

all: bc obj cov

bc : Tree.bc pmdk.bc Epoche.bc
Tree.bc : copy
	$(CXX) $(CFLAGS) -emit-llvm Tree.cpp
pmdk.bc : copy
	$(CXX) $(CFLAGS) -emit-llvm  pmdk.cpp
Epoche.bc : copy
	$(CXX) $(CFLAGS) -emit-llvm Epoche.cpp

cov: Tree.cov.o pmdk.cov.o Epoche.cov.o
Tree.cov.o : copy
	$(CXX) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping Tree.cpp -o Tree.cov.o
pmdk.cov.o : copy
	$(CXX) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping pmdk.cpp -o pmdk.cov.o
Epoche.cov.o : copy
	$(CXX) $(CFLAGS) -fprofile-instr-generate -fcoverage-mapping Epoche.cpp -o Epoche.cov.o

obj : Tree.o pmdk.o Epoche.o
Tree.o : copy
	$(CXX) $(CFLAGS) Tree.cpp
pmdk.o : copy
	$(CXX) $(CFLAGS) pmdk.cpp
Epoche.o : copy
	$(CXX) $(CFLAGS) Epoche.cpp

copy:
	@ cp $(FILES) .

clean:
	@ rm -f *.cpp *.bc *.o
