CC=$(LLVM9_BIN)/clang

MEMKIND_FLAGS=-DHAVE_CONFIG_H -I../  -I../include -I../jemalloc/obj/include -I../test/gtest_fused -DMEMKIND_DEPRECATED\(x\)=x  -fvisibility=hidden -Wall -Werror -msse4.2 -D_GNU_SOURCE -DMEMKIND_INTERNAL_API -g -O2 -fstack-protector -D_FORTIFY_SOURCE=2 

MEMKIND_BC=hbwmalloc.bc memkind.bc memkind_gbtlb.bc memkind-hbw-nodes.bc memkind_interleave.bc memkind_pmem.bc memkind_arena.bc memkind_default.bc memkind_hbw.bc memkind_hugetlb.bc memkind_log.bc

all: $(MEMKIND_BC) obj


.PHONY: all clean
%.bc: %.c
	$(CC) $(MEMKIND_FLAGS) -c -emit-llvm $<

obj:
	ar cru ../.libs/libmemkind.o ../jemalloc/obj/lib/libjemalloc_pic.a  hbwmalloc.o memkind_arena.o memkind.o memkind_default.o memkind_gbtlb.o memkind_hbw.o memkind_hugetlb.o memkind_pmem.o memkind_interleave.o memkind_log.o
	ranlib ../.libs/libmemkind.o

clean:
	rm -rf *.bc
