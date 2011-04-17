all:
	gcc -O3 -c engine.c
	gcc -O3 test.c -o test
	gcc -O3 score.c -lm -lrt -o score

clean:
	rm -f engine.o test score a.out *~
