CFLAGS=-Dtest_main=main -Wall -g -pg
LDFLAGS=-g -pg

FS_OBJ=$(patsubst %.c, %.o, $(wildcard ../*.c))
TEST_OBJ=operations-test.o tree-test.o

all: test

operations-test: $(FS_OBJ) operations-test.o
	gcc -o operations-test $(FS_OBJ) operations-test.o $(LDFLAGS)

tree-test: $(FS_OBJ) tree-test.o
	gcc -o tree-test $(FS_OBJ) tree-test.o $(LDFLAGS)

clean:
	rm -f operations-test tree-test $(TEST_OBJ) $(FS_OBJ)

test: test-operations test-tree

test-operations: operations-test
	./operations-test

test-tree: tree-test
	./tree-test

profile: clean
	make test
	gprof

coverage: CFLAGS+=-ftest-coverage -fprofile-arcs
coverage: LDFLAGS+=-ftest-coverage -fprofile-arcs
coverage: test
	gcov *.c ../*.c
