all:
	$(CC) -c pmdk.c
	$(CC) -c wort.c
	$(CC) -c example.c
	$(CC) -O3 -o example pmdk.o wort.o example.o -lpmemobj
