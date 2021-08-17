INCDIR = include
MAINDIR = main
TESTDIR = test

.PHONY: all test main include clean

all : test

test: main
	$(MAKE) -C $(TESTDIR)

main: include
	$(MAKE) -C $(MAINDIR)

include:
	$(MAKE) -C $(INCDIR)

clean:
	$(MAKE) -C $(INCDIR) clean
	$(MAKE) -C $(MAINDIR) clean
	$(MAKE) -C $(TESTDIR) clean
