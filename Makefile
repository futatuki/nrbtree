# $yfId: readindex/branches/on_the_way/btree/Makefile 56 2016-10-18 15:09:55Z futatuki $
DEBUG		?= 0
CFLAGS 		= -pipe -pthread -Wall -I/usr/local/include/python2.7 
DEBUG_CFLAGS 	= -DDEBUG=1 -fstack-protector -O0 -g
SHARED_CFLAGS	= -fPIC
LDFLAGS		+= -L/usr/local/lib
SHARED_LDFLAGS	= -shared
LIBS		+= -lpython2.7

CFLAGS		+= -DGENERIC_LIB=1

.if $(DEBUG) != 0
CFLAGS		+= $(DEBUG_CFLAGS)

all : pybtree.so check_leak

check_leak : check_leak.o btree.o
	cc $(LDFLAGS) check_leak.o btree.o -o $@

check_leak.o : check_leak.c btree.h

.else
CFLAGS		+= -O2 -DNDEBUG

all: pybtree.so

.endif


.SUFFIXES: .pyx .pxd .so

.pyx.c:
	cython $(CYTHONFLAGS) $<

pybtree.pxd : pybtree.pxd.in
	sed  -e s/@DEBUG@/$(DEBUG)/ $*.pxd.in > $@

pybtree.pyx : pybtree.pyx.in
	sed  -e s/@DEBUG@/$(DEBUG)/ $*.pyx.in > $@

pybtree.c : pybtree.pxd pybtree.pyx

pybtree.o : btree.h pybtree.c
	cc $(SHARED_CFLAGS) $(CFLAGS) -c $*.c

btree.o : btree.h btree.c
	cc $(SHARED_CFLAGS) $(CFLAGS) -c $*.c

pybtree.so : btree.o pybtree.o
	cc $(SHARED_LDFLAGS) $(LDFLAGS) $(LIBS) $> -o $@

clean :
	-rm *.o *.so pybtree.pxd pybtree.pyx pybtree.c check_leak
