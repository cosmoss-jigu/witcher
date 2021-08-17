include ../../../Makefile.main.common

NAME = main

IR_FILES += $(NAME).bc
OBJ_FILES = $(NAME).o
COV_FILES = $(NAME).cov.o

INC_DIR0 = -I../include/hot/commons/include \
						-I../include//hot/rowex/include \
						-I../include/idx/benchmark-helpers/include \
						-I../include/idx/map-helpers/include \
						-I../include/idx/content-helpers/include \
						-I../include/idx/utils/include \
						-I../include/profile-lib/include

INC_DIR1 = $(GIRI_DIR)/include/Witcher
CFLAGS += $(INC_DIR0) -I$(INC_DIR1) -Wno-deprecated

LINK_LIBS += -pthread -ltbb

# Generate $(NAME).bc for $(NAME).c
$(NAME).bc : $(NAME).cpp
	$(CC) $(CFLAGS) -DCLWB -march=native -mrtm -mcx16 -mavx -mavx2 -mbmi2 -mlzcnt -DNDEBUG $+ -o $@

# Generate $(NAME).o for $(NAME).c
$(NAME).o : $(NAME).cpp
	$(CC) -g -O0 -c $(INC_DIR0) -I$(INC_DIR1) -DCLWB -march=native -mrtm -mcx16 -mavx -mavx2 -mbmi2 -mlzcnt -DNDEBUG $+

$(NAME).cov.o : $(NAME).cpp
	$(CC) -g -O0 -c -fprofile-instr-generate -fcoverage-mapping $(INC_DIR0) -I$(INC_DIR1) -DCLWB -march=native -mrtm -mcx16 -mavx -mavx2 -mbmi2 -mlzcnt -DNDEBUG $+ -o $@
