redis = yes

include ../../Makefile.main.common

NAME = main

IR_FILES = ../lib/sentinel.bc ../lib/hyperloglog.bc ../lib/zmalloc.bc ../lib/cluster.bc ../lib/debug.bc ../lib/t_zset.bc ../lib/t_hash.bc ../lib/dict.bc ../lib/quicklist.bc ../lib/ae.bc ../lib/pmem.bc ../lib/t_list.bc ../lib/setproctitle.bc ../lib/sha1.bc ../lib/bio.bc ../lib/crc16.bc ../lib/geo.bc ../lib/rio.bc ../lib/lzf_c.bc ../lib/latency.bc ../lib/rand.bc ../lib/server.bc ../lib/t_string.bc ../lib/object.bc ../lib/pqsort.bc ../lib/multi.bc ../lib/lzf_d.bc ../lib/intset.bc ../lib/sds.bc ../lib/redis-check-rdb.bc ../lib/endianconv.bc ../lib/db.bc ../lib/aof.bc ../lib/pubsub.bc ../lib/ziplist.bc ../lib/bitops.bc ../lib/slowlog.bc ../lib/release.bc ../lib/syncio.bc ../lib/blocked.bc ../lib/util.bc ../lib/memtest.bc ../lib/sparkline.bc ../lib/zipmap.bc ../lib/sort.bc ../lib/adlist.bc ../lib/rdb.bc ../lib/scripting.bc ../lib/crc64.bc ../lib/t_set.bc ../lib/notify.bc ../lib/anet.bc ../lib/replication.bc ../lib/networking.bc ../lib/config.bc $(WITCHER_HOME)/benchmark/pmdk-1.8-deps/*.bc

INC_DIR0 = ../include
INC_DIR1 = $(GIRI_DIR)/../include/Witcher
CFLAGS += -I$(INC_DIR0) -I$(INC_DIR1) -Wno-deprecated

REDIS_GEOHASH_OBJ=../deps/geohash-int/geohash.o ../deps/geohash-int/geohash_helper.o
LINK_LIBS += ../deps/hiredis/libhiredis.a ../deps/lua/src/liblua.a $(REDIS_GEOHASH_OBJ) -ldl -lpthread ../lib/witcher.o
