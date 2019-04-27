# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.
#

BYUNETID = son1124
VERSION = 1
HANDINDIR = /users/faculty/snell/CS324/handin/Fall2018/ProxyLab2


CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

log.o: log.c log.h
	$(CC) $(CFLAGS) -c log.c

sbuf.o: sbuf.c sbuf.h
	$(CC) $(CFLAGS) -c sbuf.c

cache.o: cache.c cache.h
	$(CC) $(CFLAGS) -c cache.c

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o sbuf.o cache.o log.o
	$(CC) $(CFLAGS) proxy.o csapp.o sbuf.o cache.o log.o -o proxy $(LDFLAGS)

valgrind:
	valgrind --leak-check=full -v ./proxy 80

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*"; cp proxylab-handin.tar $(HANDINDIR)/$(BYUNETID)-$(VERSION)-proxylab-handin.tar)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz

