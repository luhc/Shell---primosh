
CC=gcc
CFLAGS=-Wall

PROG=primosh
OBJS=main.o jobs.o syserror.o processo.o

$(PROG): $(OBJS)
	$(CC) $^ -o $@
	
jobs.o: jobs.h syserror.h
processo.o: jobs.h syserror.h
syserror.o: syserror.h
main.o: jobs.h processo.h syserror.h

clean:
	rm $(OBJS) $(PROG)
