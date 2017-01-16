all: 	util.o feat.o dot.o srails_disc genproj lsh 

#OPT = -O4 -std=c99 -Wall
#OPT = -O4 -pg -std=c99 -Wall
OPT = -O4 -g -std=c99 -Wall

util.o: util.c util.h Makefile 
	gcc ${OPT} -c util.c 

feat.o: feat.c feat.h Makefile
	gcc ${OPT} -c feat.c

dot.o: dot.c dot.h Makefile
	gcc ${OPT} -c dot.c

srails_disc: srails_disc.c dot.o feat.o util.o Makefile signature.c
	gcc ${OPT} -o srails_disc srails_disc.c signature.c -lm util.o dot.o feat.o 

genproj: Makefile genproj.c util.o
	gcc ${OPT}  -o genproj genproj.c util.o -lm 

lsh: Makefile lsh.c util.o
	gcc ${OPT}  -o lsh lsh.c util.o -lm 

clean:
	rm -f *~ *.o srails_disc genproj lsh

