CC = gcc
COPTS = -c -fPIC -g
LD = gcc
LDOPTS = -shared -lpthread -lrt
LIBNAME = jw.so

$(LIBNAME): jw.o
	$(LD) -o $(LIBNAME) jw.o $(LDOPTS)

jw.o: JakWorkers.c JakWorkers.h
	$(CC) $(COPTS) -o jw.o JakWorkers.c

clean:
	*.o $(LIBNAME)
