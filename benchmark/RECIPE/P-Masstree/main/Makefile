include ../../../Makefile.main.common

NAME = main

LIB_DIR = ../lib
SRC_CPP = $(NAME).cpp $(LIB_DIR)/pmdk.cpp
IR_FILES += $(SRC_CPP:%.cpp=%.bc)
OBJ_FILES = $(SRC_CPP:%.cpp=%.o)
COV_FILES = $(SRC_CPP:%.cpp=%.cov.o)

INC_DIR0 = ../include
INC_DIR1 = $(GIRI_DIR)/include/Witcher
CFLAGS += -I$(INC_DIR0) -I$(INC_DIR1) -Wno-deprecated -DCLWB


# Generate $(NAME).bc for $(NAME).c
$(NAME).bc : $(NAME).cpp
	$(CC) $(CFLAGS) $+ -o $@

# Generate $(NAME).o for $(NAME).c
$(NAME).o : $(NAME).cpp
	$(CC) -g -O0 -c -I$(INC_DIR0) -I$(INC_DIR1) -DCLWB $+

$(NAME).cov.o : $(NAME).cpp
	$(CC) -g -O0 -c -fprofile-instr-generate -fcoverage-mapping -I$(INC_DIR0) -I$(INC_DIR1) -DCLWB $+ -o $@
