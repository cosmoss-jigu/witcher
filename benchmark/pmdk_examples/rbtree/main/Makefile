include ../../../Makefile.main.common

NAME = main

LIB_DIR = ../lib
SRC_C = $(NAME).c $(LIB_DIR)/rbtree_map.c
IR_FILES += $(SRC_C:%.c=%.bc)
OBJ_FILES = $(SRC_C:%.c=%.o)

INC_DIR0 = ../include
INC_DIR1 = $(GIRI_DIR)/include/Witcher
CFLAGS += -I$(INC_DIR0) -I$(INC_DIR1) -Wno-deprecated

# Generate $(NAME).bc for $(NAME).c
$(NAME).bc : $(NAME).c
	$(CC) $(CFLAGS) $+ -o $@

# Generate $(NAME).o for $(NAME).c
$(NAME).o : $(NAME).c
	$(CC) -g -c -I$(INC_DIR0) -I$(INC_DIR1) $+
