all:
	$(CC) -c pmdk.c -g
	$(CC) -c woart.c -g
	$(CC) -c example.c -g
	$(CC) -O3 -o example pmdk.o woart.o example.o -lpmemobj
