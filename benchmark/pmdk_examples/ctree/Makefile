INCDIR = include
LIBDIR = lib
MAINDIR = main
TESTDIR = test

.PHONY: all test main include lib clean

all : test

test: main
	$(MAKE) -C $(TESTDIR)

main: lib
	$(MAKE) -C $(MAINDIR)

lib : include
	$(MAKE) -C $(LIBDIR)

include:
	$(MAKE) -C $(INCDIR)

clean:
	$(MAKE) -C $(INCDIR) clean
	$(MAKE) -C $(LIBDIR) clean
	$(MAKE) -C $(MAINDIR) clean
	$(MAKE) -C $(TESTDIR) clean
