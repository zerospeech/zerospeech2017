all: 	util.o icsilog.o wordsim compute_distrib

#OPT = -O4 -std=c99 -Wall
OPT = -O4 -std=c99 -g -Wall

util.o: util.c util.h Makefile
	gcc ${OPT} -c util.c

icsilog.o: icsilog.c icsilog.h Makefile
	gcc ${OPT} -c icsilog.c

wordsim: wordsim.c util.o Makefile 
	gcc ${OPT}  -o wordsim wordsim.c -lm util.o icsilog.o

compute_distrib: compute_distrib.c util.o Makefile 
	gcc ${OPT}  -o compute_distrib compute_distrib.c -lm util.o

clean:
	rm -f *~ *.o wordsim compute_distrib

