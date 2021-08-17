all: writer reader

writer: test_writer.o level_hashing.o hash.o pflush.o log.o pmdk.o
	gcc -g -o writer test_writer.o level_hashing.o hash.o pflush.o log.o pmdk.o -lm -lpmemobj

test_writer.o: test_writer.c level_hashing.h level_hashing_printer.h
	gcc -g -c test_writer.c

reader: test_reader.o level_hashing.o hash.o pflush.o log.o pmdk.o
	gcc -g -o reader test_reader.o level_hashing.o hash.o pflush.o log.o pmdk.o -lm -lpmemobj

test_reader.o: test_reader.c level_hashing.h level_hashing_printer.h
	gcc -g -c test_reader.c

plevel: test.o level_hashing.o hash.o pflush.o log.o pmdk.o
	gcc -g -o plevel test.o level_hashing.o hash.o pflush.o log.o pmdk.o -lm -lpmemobj

hash.o: hash.c hash.h
	gcc -g -c hash.c

level_hashing.o: level_hashing.c level_hashing.h pmdk.h
	gcc -g -c level_hashing.c

test.o: test.c level_hashing.h
	gcc -g -c test.c

pflush.o: pflush.c pflush.h
	gcc -g -c pflush.c

log.o: log.c log.h
	gcc -g -c log.c

pmdk.o: pmdk.c pmdk.h
	gcc -g -c pmdk.c

clean:
	rm -rf *.o writer reader
