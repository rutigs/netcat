all: ncTh ncP

#The following lines contain the generic build options
CC=gcc
CPPFLAGS=
CFLAGS=-g

TH_CLIBS=-pthread

THREADOBJS=ncTh.o usage.o common.o

POLL_CLIBS=

POLLOBJS=ncP.o usage.o common.o

ncTh: $(THREADOBJS)
	$(CC) -o ncTh $(THREADOBJS)  $(TH_CLIBS)

ncP: $(POLLOBJS)
	$(CC) -o ncP $(POLLOBJS) $(POLL_CLIBS)

clean:
	rm -f *.o
	rm -f ncP ncTh


