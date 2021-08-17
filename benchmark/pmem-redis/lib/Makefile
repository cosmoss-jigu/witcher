CC=$(LLVM9_BIN)/clang
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
OPTIMIZATION?=-O0
DEPENDENCY_TARGETS=hiredis linenoise lua
NODEPS:=clean 

SRC_FILES = ../../../third_party/pmem-redis/src/*.h ../../../third_party/pmem-redis/src/*.c  ../../../third_party/pmem-redis/redis.conf

# Default settings
STD=-std=gnu99 -pedantic -DREDIS_STATIC=''
WARN=-Wall -W -Wno-missing-field-initializers -Wno-implicit-fallthrough -Wno-expansion-to-defined -Wno-maybe-uninitialized
OPT=$(OPTIMIZATION)


# Default allocator defaults to Jemalloc if it's not an ARM
MALLOC=jemalloc

# Override default settings if possible
-include .make-settings

FINAL_CFLAGS=$(STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS) $(REDIS_CFLAGS)
FINAL_LDFLAGS=$(LDFLAGS) $(REDIS_LDFLAGS) $(DEBUG)
FINAL_LIBS=-lm
DEBUG=-g -ggdb

# All the other OSes (notably Linux)
FINAL_LDFLAGS+= -rdynamic
FINAL_LIBS+=-ldl -pthread
# Include paths to dependencies
FINAL_CFLAGS+= -I../deps/hiredis -I../deps/linenoise -I../deps/lua/src


DEPENDENCY_TARGETS+= pmdk memkind
FINAL_CFLAGS+= -DUSE_NVM -I../deps/pmdk/src/include -I../deps/memkind/include -I../include
FINAL_LIBS+= ../deps/pmdk/src/nondebug/libpmem.a
FINAL_LIBS+= ../deps/memkind/jemalloc/obj/lib/libjemalloc.a ../deps/memkind/.libs/libmemkind.o
FINAL_LIBS+= -lnuma
AEP_COW = yes
SUPPORT_PBA = yes
USE_AOFGUARD = yes
FAST_SDSFREE = yes

FINAL_CFLAGS += -DAEP_COW

DEPENDENCY_TARGETS+= jemalloc
FINAL_CFLAGS+= -DUSE_JEMALLOC -I../deps/jemalloc/include
FINAL_LIBS+= ../deps/jemalloc/lib/libjemalloc.a
FINAL_CFLAGS+= -DJE_PREFIX=je_

DEPENDENCY_TARGETS += jemallocat
FINAL_CFLAGS += -DSUPPORT_PBA -I../deps/jemallocat/include
FINAL_LIBS += ../deps/jemallocat/lib/libjemallocat.a

DEPENDENCY_TARGETS += aofguard
FINAL_CFLAGS += -DUSE_AOFGUARD -I../deps/aofguard/include
FINAL_LIBS += ../deps/aofguard/lib/libaofguard.a
FINAL_CFLAGS += -DFAST_SDSFREE

REDIS_CC=$(QUIET_CC)$(CC) $(FINAL_CFLAGS)
REDIS_LD=$(QUIET_LINK)$(CC) $(FINAL_LDFLAGS)
REDIS_INSTALL=$(QUIET_INSTALL)$(INSTALL)

CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"
#V=a

ifndef V
QUIET_CC = @printf '    %b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK = @printf '    %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_INSTALL = @printf '    %b %b\n' $(LINKCOLOR)INSTALL$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
endif

REDIS_SERVER_NAME=redis-server
REDIS_SERVER_OBJ=witcher.o adlist.o quicklist.o ae.o anet.o dict.o server.o sds.o zmalloc.o lzf_c.o lzf_d.o pqsort.o zipmap.o sha1.o ziplist.o release.o networking.o util.o object.o db.o replication.o rdb.o t_string.o t_list.o t_set.o t_zset.o t_hash.o config.o aof.o pubsub.o multi.o debug.o sort.o intset.o syncio.o cluster.o crc16.o endianconv.o slowlog.o scripting.o bio.o rio.o rand.o memtest.o crc64.o bitops.o sentinel.o notify.o setproctitle.o blocked.o hyperloglog.o latency.o sparkline.o redis-check-rdb.o redis-check-aof.o geo.o lazyfree.o module.o evict.o expire.o geohash.o geohash_helper.o childinfo.o defrag.o siphash.o rax.o nvm.o
REDIS_BC=witcher.bc adlist.bc quicklist.bc ae.bc anet.bc dict.bc server.bc sds.bc zmalloc.bc lzf_c.bc lzf_d.bc pqsort.bc zipmap.bc sha1.bc ziplist.bc release.bc networking.bc util.bc object.bc db.bc replication.bc rdb.bc t_string.bc t_list.bc t_set.bc t_zset.bc t_hash.bc config.bc aof.bc pubsub.bc multi.bc debug.bc sort.bc intset.bc syncio.bc cluster.bc crc16.bc endianconv.bc slowlog.bc scripting.bc bio.bc rio.bc rand.bc memtest.bc crc64.bc bitops.bc sentinel.bc notify.bc setproctitle.bc blocked.bc hyperloglog.bc latency.bc sparkline.bc redis-check-rdb.bc redis-check-aof.bc geo.bc lazyfree.bc module.bc evict.bc expire.bc geohash.bc geohash_helper.bc childinfo.bc defrag.bc siphash.bc rax.bc nvm.bc nvm_cow.bc
ifeq ($(AEP_COW),yes)
    REDIS_SERVER_OBJ += nvm_cow.o
endif

REDIS_CLI_NAME=redis-cli
REDIS_CLI_OBJ=anet.o adlist.o redis-cli.o zmalloc.o release.o anet.o ae.o crc64.o
REDIS_BENCHMARK_NAME=redis-benchmark
REDIS_BENCHMARK_SEQ_NAME=redis-benchmark-seq
REDIS_BENCHMARK_OBJ=ae.o anet.o redis-benchmark.o adlist.o zmalloc.o redis-benchmark.o
REDIS_BENCHMARK_SEQ_OBJ=ae.o anet.o redis-benchmark-seq.o adlist.o zmalloc.o redis-benchmark-seq.o
REDIS_CHECK_RDB_NAME=redis-check-rdb
REDIS_CHECK_AOF_NAME=redis-check-aof
#REDIS_CHECK_AOF_OBJ=redis-check-aof.o


TARGETS = copy $(REDIS_SERVER_NAME) $(REDIS_BC)
all:  $(TARGETS)
	@echo ""
	@echo "Hint: It's a good idea to run 'make test' ;)"
	@echo ""
	cp $(REDIS_SERVER_NAME) ../main/main.exe
Makefile.dep:
	-$(REDIS_CC) -MM *.c > Makefile.dep 2> /dev/null || true

ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
-include Makefile.dep
endif

.PHONY: all
copy:
	cp $(SRC_FILES) .

.PHONY: copy

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
	-(cd ../deps && $(MAKE) $(DEPENDENCY_TARGETS))

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
	$(REDIS_LD) -o $@ $^ ../deps/hiredis/libhiredis.a ../deps/lua/src/liblua.a $(FINAL_LIBS)

# Because the jemalloc.h header is generated as a part of the jemalloc build,
# building it should complete before building any other object. Instead of
# depending on a single artifact, build all dependencies first.
%.o: %.c .make-prerequisites
	$(REDIS_CC) -c $<
%.bc: %.c .make-prerequisites
	$(REDIS_CC) -c -emit-llvm $<
clean:
	rm -rf $(REDIS_SERVER_NAME) *.o *.c *.h *.bc *.conf dump.rdb .make-settings .make-prerequisites 

.PHONY: clean

noopt:
	$(MAKE) OPTIMIZATION="-O0"

valgrind:
	$(MAKE) OPTIMIZATION="-O0" MALLOC="libc"

helgrind:
	$(MAKE) OPTIMIZATION="-O0" MALLOC="libc" CFLAGS="-D__ATOMIC_VAR_FORCE_SYNC_MACROS"

src/help.h:
	@../utils/generate-command-help.rb > help.h


