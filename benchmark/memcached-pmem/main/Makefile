redis = yes

include ../../Makefile.main.common

NAME = main

INC_DIR0 = ../include
INC_DIR1 = $(GIRI_DIR)/../include/Witcher
CFLAGS += -I$(INC_DIR0) -I$(INC_DIR1) -Wno-deprecated

LINK_LIBS += -pthread -levent
