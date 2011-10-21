#
# Makefile for the compiler
#
# Copyright (C) 2003, Edmund Horner.
#

all:
	$(MAKE) -C src
	cp src/ecc .

clean:
	$(MAKE) -C src clean

test: all
	for x in `ls examples/*.e` ; do echo " *" $$x ; ./ecc $$x > /dev/null ; done
