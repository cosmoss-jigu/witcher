CC=$(LLVM9_BIN)/clang
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
OPTIMIZATION?=-O0
DEPENDENCY_TARGETS=hiredis linenoise lua geohash-int
SRC_FILES = ../../../third_party/redis-3.2-nvml/src/*.h ../../../third_party/redis-3.2-nvml/src/*.c  ../../../third_party/redis-3.2-nvml/src/*.sup ../../../third_party/redis-3.2-nvml/redis.conf

# Default settings
STD=-std=gnu99 -pedantic -DREDIS_STATIC=''
WARN=-Wall -W
OPT=$(OPTIMIZATION)

# Default allocator
MALLOC=libc

# Override default settings if possible
-include .make-settings

FINAL_CFLAGS=$(STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS) $(REDIS_CFLAGS) -I../deps/geohash-int 
FINAL_LDFLAGS=$(LDFLAGS) $(REDIS_LDFLAGS) $(DEBUG)
FINAL_LIBS=-lm
DEBUG=-g -ggdb

FINAL_LIBS+= -lpthread

# Include paths to dependencies
FINAL_CFLAGS+= -I../deps/hiredis -I../deps/linenoise -I../deps/lua/src


FINAL_CFLAGS+= -I$(WITCHER_HOME)/pmdk-1.8/src/include -I$(WITCHER_HOME)/pmdk-1.8/src/libpmemobj -I$(WITCHER_HOME)/pmdk-1.8/src/common -DUSE_PMDK
FINAL_LIBS+= -lpmemobj -lpmem
FINAL_LIBS+= -ldl

REDIS_CC=$(QUIET_CC)$(CC) $(FINAL_CFLAGS)
REDIS_LD=$(QUIET_LINK)$(CC) $(FINAL_LDFLAGS)

CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

ifndef V
QUIET_CC = @printf '    %b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK = @printf '    %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_INSTALL = @printf '    %b %b\n' $(LINKCOLOR)INSTALL$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
endif

REDIS_SERVER_NAME=redis-server
REDIS_SERVER_OBJ=witcher.o adlist.o quicklist.o ae.o anet.o dict.o server.o sds.o zmalloc.o lzf_c.o lzf_d.o pqsort.o zipmap.o sha1.o ziplist.o release.o networking.o util.o object.o db.o replication.o rdb.o t_string.o t_list.o t_set.o t_zset.o t_hash.o config.o aof.o pubsub.o multi.o debug.o sort.o intset.o syncio.o cluster.o crc16.o endianconv.o slowlog.o scripting.o bio.o rio.o rand.o memtest.o crc64.o bitops.o sentinel.o notify.o setproctitle.o blocked.o hyperloglog.o latency.o sparkline.o redis-check-rdb.o geo.o
REDIS_BC=witcher.bc pmem.bc adlist.bc quicklist.bc ae.bc anet.bc dict.bc server.bc sds.bc zmalloc.bc lzf_c.bc lzf_d.bc pqsort.bc zipmap.bc sha1.bc ziplist.bc release.bc networking.bc util.bc object.bc db.bc replication.bc rdb.bc t_string.bc t_list.bc t_set.bc t_zset.bc t_hash.bc config.bc aof.bc pubsub.bc multi.bc debug.bc sort.bc intset.bc syncio.bc cluster.bc crc16.bc endianconv.bc slowlog.bc scripting.bc bio.bc rio.bc rand.bc memtest.bc crc64.bc bitops.bc sentinel.bc notify.bc setproctitle.bc blocked.bc hyperloglog.bc latency.bc sparkline.bc redis-check-rdb.bc geo.bc
REDIS_GEOHASH_OBJ=../deps/geohash-int/geohash.o ../deps/geohash-int/geohash_helper.o
REDIS_SERVER_OBJ+= pmem.o

all: copy $(REDIS_SERVER_NAME) $(REDIS_BC)
	@echo ""
	@echo "Hint: It's a good idea to run 'make test' ;)"
	@echo ""
	cp $(REDIS_SERVER_NAME) ../main/main.exe

.PHONY: all

copy:
	cp $(SRC_FILES) .
# Deps (use make dep to generate this)
include Makefile.dep

.PHONY: copy

dep:
	$(REDIS_CC) -MM *.c > Makefile.dep

.PHONY: dep

persist-settings: 
	echo STD=$(STD) >> .make-settings
	echo WARN=$(WARN) >> .make-settings
	echo OPT=$(OPT) >> .make-settings
	echo MALLOC=$(MALLOC) >> .make-settings
	echo CFLAGS=$(CFLAGS) >> .make-settings
	echo LDFLAGS=$(LDFLAGS) >> .make-settings
	echo REDIS_CFLAGS=$(REDIS_CFLAGS) >> .make-settings
	echo REDIS_LDFLAGS=$(REDIS_LDFLAGS) >> .make-settings
	echo PREV_FINAL_CFLAGS=$(FINAL_CFLAGS) >> .make-settings
	echo PREV_FINAL_LDFLAGS=$(FINAL_LDFLAGS) >> .make-settings

.PHONY: persist-settings

# Prerequisites target
.make-prerequisites:
	@touch $@

# Clean everything, persist settings and build dependencies if anything changed
ifneq ($(strip $(PREV_FINAL_CFLAGS)), $(strip $(FINAL_CFLAGS)))
.make-prerequisites: persist-settings
endif

ifneq ($(strip $(PREV_FINAL_LDFLAGS)), $(strip $(FINAL_LDFLAGS)))
.make-prerequisites: persist-settings
endif

# redis-server
$(REDIS_SERVER_NAME): $(REDIS_SERVER_OBJ)
	$(REDIS_LD) -o $@ $^ ../deps/hiredis/libhiredis.a ../deps/lua/src/liblua.a $(REDIS_GEOHASH_OBJ) $(FINAL_LIBS)


# Because the jemalloc.h header is generated as a part of the jemalloc build,
# building it should complete before building any other object. Instead of
# depending on a single artifact, build all dependencies first.
%.o: %.c .make-prerequisites
	$(REDIS_CC) -c $<
%.bc: %.c .make-prerequisites
	$(REDIS_CC) -c -emit-llvm $<

clean:
	@ rm -rf *.c *.h *.bc *.rb *.sup $(REDIS_SERVER_NAME)  *.o *.conf 

.PHONY: clean

noopt:
	$(MAKE) OPTIMIZATION="-O0"

valgrind:
	$(MAKE) OPTIMIZATION="-O0" MALLOC="libc"

src/help.h:
	@../utils/generate-command-help.rb > help.h

