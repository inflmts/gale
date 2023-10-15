# Makefile for Galcore

ifeq ($(ENV),release)
env_flags = -O3
else
env_flags = -DGALE_DEBUG
endif

CFLAGS = -Wall $(env_flags)

.PHONY: all
all: galconf galinst

galconf: common.o astr.o config.o galconf.o

galinst: common.o astr.o config.o \
	galinst-base.o galinst-manifest.o \
	galinst-log.o galinst.o

common.o: common.c
astr.o: astr.c
config.o: config.c
galconf.o: galconf.c
galinst-base.o: galinst-base.c
galinst-manifest.o: galinst-manifest.c
galinst-log.o: galinst-log.c
galinst.o: galinst.c

.PHONY: clean
clean:
	rm -f galconf galinst *.o
