UTILDIR = util
SRCDIR = src
MAINDIR = main
TESTDIR = test

.PHONY: all test main src util clean

all : test

test: main
	$(MAKE) -C $(TESTDIR)

main: src
	$(MAKE) -C $(MAINDIR)

src: util
	$(MAKE) -C $(SRCDIR)

util:
	$(MAKE) -C $(UTILDIR)

clean:
	$(MAKE) -C $(UTILDIR) clean
	$(MAKE) -C $(SRCDIR) clean
	$(MAKE) -C $(MAINDIR) clean
	$(MAKE) -C $(TESTDIR) clean
