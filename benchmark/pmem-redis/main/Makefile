redis = yes

include ../../Makefile.main.common

NAME = main

IR_FILES = ../lib/witcher.bc ../lib/adlist.bc ../lib/quicklist.bc ../lib/ae.bc ../lib/anet.bc ../lib/dict.bc ../lib/server.bc ../lib/sds.bc ../lib/zmalloc.bc ../lib/lzf_c.bc ../lib/lzf_d.bc ../lib/pqsort.bc ../lib/zipmap.bc ../lib/sha1.bc ../lib/ziplist.bc ../lib/release.bc ../lib/networking.bc ../lib/util.bc ../lib/object.bc ../lib/db.bc ../lib/replication.bc ../lib/rdb.bc ../lib/t_string.bc ../lib/t_list.bc ../lib/t_set.bc ../lib/t_zset.bc ../lib/t_hash.bc ../lib/config.bc ../lib/aof.bc ../lib/pubsub.bc ../lib/multi.bc ../lib/debug.bc ../lib/sort.bc ../lib/intset.bc ../lib/syncio.bc ../lib/cluster.bc ../lib/crc16.bc ../lib/endianconv.bc ../lib/slowlog.bc ../lib/scripting.bc ../lib/bio.bc ../lib/rio.bc ../lib/rand.bc ../lib/memtest.bc ../lib/crc64.bc ../lib/bitops.bc ../lib/sentinel.bc ../lib/notify.bc ../lib/setproctitle.bc ../lib/blocked.bc ../lib/hyperloglog.bc ../lib/latency.bc ../lib/sparkline.bc ../lib/redis-check-rdb.bc ../lib/redis-check-aof.bc ../lib/geo.bc ../lib/lazyfree.bc ../lib/module.bc ../lib/evict.bc ../lib/expire.bc ../lib/geohash.bc ../lib/geohash_helper.bc ../lib/childinfo.bc ../lib/defrag.bc ../lib/siphash.bc ../lib/rax.bc ../lib/nvm.bc ../lib/nvm_cow.bc
IR_FILES += ../deps/memkind/src/hbwmalloc.bc ../deps/memkind/src/memkind_arena.bc ../deps/memkind/src/memkind.bc ../deps/memkind/src/memkind_default.bc ../deps/memkind/src/memkind_gbtlb.bc ../deps/memkind/src/memkind_hbw.bc ../deps/memkind/src/memkind_hugetlb.bc ../deps/memkind/src/memkind_interleave.bc ../deps/memkind/src/memkind_log.bc ../deps/memkind/src/memkind_pmem.bc

INC_DIR0 = ../include
INC_DIR1 = $(GIRI_DIR)/../include/Witcher
CFLAGS += -I$(INC_DIR0) -I$(INC_DIR1) -Wno-deprecated

LINK_LIBS = ../deps/pmdk/src/nondebug/libpmem.a
LINK_LIBS += -lpmemobj
LINK_LIBS += ../deps/memkind/jemalloc/obj/lib/libjemalloc.a
LINK_LIBS += ../deps/jemalloc/lib/libjemalloc.a
LINK_LIBS += ../deps/jemallocat/lib/libjemallocat.a
LINK_LIBS += ../deps/aofguard/lib/libaofguard.a
LINK_LIBS += ../deps/hiredis/libhiredis.a
LINK_LIBS += ../deps/lua/src/liblua.a
LINK_LIBS += -lm -ldl -pthread -lnuma
