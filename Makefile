CC = gcc
COPTS = -c -fPIC -g
LD = gcc
LDOPTS = -shared -lpthread
LIBNAME = jw.so
VERSION = 0.1

$(LIBNAME): jw.o
	$(LD) -o $(LIBNAME) jw.o $(LDOPTS)

.PHONY: dist
dist: $(LIBNAME) 
	mkdir -p dist/lib/ dist/include/ dist/doc/
	cp JakWorkers.h dist/include/
	cp $(LIBNAME) dist/lib/
	cp README.md LICENSE dist/doc/
	tar cjvf jw-$(VERSION).tbz dist/

jw.o: JakWorkers.c JakWorkers.h
	$(CC) $(COPTS) -o jw.o JakWorkers.c

clean:
	rm -rf *.o $(LIBNAME) dist/
