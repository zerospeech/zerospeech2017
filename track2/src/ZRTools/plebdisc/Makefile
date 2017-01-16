all: 	util.o feat.o dot.o dotkws.o plebdisc plebkws build_index genproj lsh standfeat rescore_singlepair_dtw

OPT = -O4 -std=c99 -Wall
#OPT = -O4 -pg -std=c99 -Wall
#OPT = -O4 -g -std=c99 -Wall

install: plebdisc plebkws build_index genproj lsh standfeat rescore_singlepair_dtw
	install -m 0755 $^ $(DESTDIR)

util.o: util.c util.h Makefile 
	gcc ${OPT} -c util.c 

feat.o: feat.c feat.h Makefile
	gcc ${OPT} -c feat.c

dot.o: dot.c dot.h Makefile
	gcc ${OPT} -c dot.c

dotkws.o: dotkws.c dotkws.h Makefile
	gcc ${OPT} -c dotkws.c

plebdisc: plebdisc.c dot.o feat.o util.o Makefile score_matches.c signature.c
	gcc ${OPT} -o plebdisc score_matches.c plebdisc.c signature.c -lm util.o dot.o feat.o -lm

plebkws: plebkws.c dotkws.o feat.o util.o Makefile score_matches.c signature.c
	gcc ${OPT} -D INDEXMODE -o plebkws score_matches.c plebkws.c signature.c -lm util.o dotkws.o feat.o 

build_index: build_index.c dot.o util.o Makefile signature.c
	gcc ${OPT} -D INDEXMODE -o build_index build_index.c -lm util.o signature.c -lm

genproj: Makefile genproj.c util.o
	gcc ${OPT}  -o genproj genproj.c -lm util.o -lm

lsh: Makefile lsh.c util.o
	gcc ${OPT}  -o lsh lsh.c util.o -lm 

standfeat: Makefile standfeat.c util.o
	gcc ${OPT}  -o standfeat standfeat.c util.o -lm 

icsilog.o: icsilog.c icsilog.h Makefile
	gcc ${OPT} -c icsilog.c

rescore_dtw: Makefile rescore_dtw.c util.o icsilog.o
	gcc ${OPT}  -o rescore_dtw rescore_dtw.c util.o icsilog.o -lm 

rescore_singlepair_dtw: Makefile rescore_singlepair_dtw.c util.o icsilog.o
	gcc ${OPT}  -o rescore_singlepair_dtw rescore_singlepair_dtw.c util.o icsilog.o -lm 

clean:
	rm -f *~ *.o plebdisc genproj lst standfeat plebkws build_index lsh standfeat rescore_singlepair_dtw

