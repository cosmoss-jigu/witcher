include ../../Makefile.main.common

NAME = main

LIB_DIR = ../lib
SRC_CPP = $(NAME).cpp
SRC_C = $(LIB_DIR)/pmdk.c
IR_FILES += $(SRC_CPP:%.cpp=%.bc) $(SRC_C:%.c=%.bc)
OBJ_FILES = $(SRC_CPP:%.cpp=%.o) $(SRC_C:%.c=%.o)
COV_FILES = $(SRC_CPP:%.cpp=%.cov.o) $(SRC_C:%.c=%.cov.o)

INC_DIR0 = ../include
INC_DIR1 = $(GIRI_DIR)/include/Witcher
CFLAGS += -I$(INC_DIR0) -I$(INC_DIR1) -Wno-deprecated -std=c++11

# Generate $(NAME).bc for $(NAME).c
$(NAME).bc : $(NAME).cpp
	$(CXX) $(CFLAGS) $+ -o $@

# Generate $(NAME).o for $(NAME).c
$(NAME).o : $(NAME).cpp
	$(CXX) -g -c -std=c++11 -I$(INC_DIR0) -I$(INC_DIR1) $+

$(NAME).cov.o : $(NAME).cpp
	$(CC) -g -c -std=c++11 -fprofile-instr-generate -fcoverage-mapping -I$(INC_DIR0) -I$(INC_DIR1) $+ -o $@
