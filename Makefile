CC = gcc
COPTS = -c -fPIC -g
LD = gcc
LDOPTS = -shared -lpthread -lrt
LIBNAME = libjw.so
VERSION = 1.1

$(LIBNAME): jw.o
	$(LD) -o $(LIBNAME) jw.o $(LDOPTS)

.PHONY: dist
dist: $(LIBNAME) 
	mkdir -p dist/lib/ dist/include/ dist/doc/ dist/src/
	cp JakWorkers.h dist/include/
	cp JakWorkers.c JakWorkers.h Makefile dist/src/
	cp $(LIBNAME) dist/lib/
	cp README.md LICENSE dist/doc/
	tar cjvf jw-$(VERSION).tbz dist/

jw.o: JakWorkers.c JakWorkers.h
	$(CC) $(COPTS) -o jw.o JakWorkers.c

clean:
	rm -rf *.o $(LIBNAME) dist/ jw-*.tbz test_*.bin

test_farray.bin: test/test_farray.c $(LIBNAME)
	gcc -g -o test_farray.bin test/test_farray.c -l:$(LIBNAME) -lm -Wl,-rpath=.

test_interactive.bin: test/test_interactive.c $(LIBNAME)
	gcc -g -o test_interactive.bin test/test_interactive.c -l:$(LIBNAME) -lm -Wl,-rpath=.

.PHONY: all_tests
all_tests: test_farray.bin test_interactive.bin
	./test_farray.bin
	test/test_interactive_launcher.sh
