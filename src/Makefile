
release_hdr := $(shell sh -c './mkreleasehdr.sh')
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
OPTIMIZATION?=-O2

#-pedantic编译选项并不能保证被编译程序与ANSI/ISO C标准的完全兼容，它仅仅只能用来帮助Linux程序员离这个目标越来越近
STD=-std=c99 -pedantic
WARN=-Wall -W
OPT=$(OPTIMIZATION)

ifeq ($(uname_S),Linux)
	MALLOC=jemalloc
else
	MALLOC=libc
endif

ifeq ($(USE_JEMALLOC),yes)
	MALLOC=jemalloc
endif

ifeq ($(USE_JEMALLOC),no)
	MALLOC=libc
endif

#引入配置文件
-include .make-settings

FINAL_CFLAGS=$(STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS) $(REDIS_CFLAGS)
FINAL_LDFLAGS=$(LDFLAGS) $(REDIS_LDFLAGS) $(DEBUG)
FINAL_LIBS=-lm
DEBUG=-g -ggdb

FINAL_LDFLAGS+= -rdynamic
FINAL_LIBS+= -pthread

ifeq ($(MALLOC),jemalloc) 
	DEPENDENCY_TARGETS+= jemalloc
	FINAL_CFLAGS+= -DUSE_JEMALLOC -I../deps/jemalloc/include
	FINAL_LIBS+= ../deps/jemalloc/lib/libjemalloc.a -ldl
endif

REDIS_CC=$(QUIET_CC) $(CC) $(FINAL_CFLAGS)
REDIS_LD=$(QUIET_LINK) $(CC) $(FINAL_LDFLAGS)

CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

ifndef V
QUIET_CC = @printf '	%b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK = @printf '	%b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
endif

all: redis test testsha1

.PHONY: all

include Makefile.dep

.make-prerequisites:
	@touch $@

test: test.o zmalloc.o adlist.o dict.o
	$(REDIS_LD) -o $@ $^ $(FINAL_LIBS)

testsha1: testsha1.o sha1.o
	$(REDIS_LD) -o $@ $^ $(FINAL_LIBS)
	
redis: redis.o setproctitle.o zmalloc.o dict.o debug.o release.o crc64.o sds.o config.o util.o
	$(REDIS_LD) -o $@ $^ $(FINAL_LIBS)

%.o: %.c .make-prerequisites
	$(REDIS_CC) -c $<

dep:
	$(REDIS_CC) -MM *.c > Makefile.dep 

.PHONY: dep

test-sds: sds.c sds.h
	$(REDIS_CC) sds.c zmalloc.c -DSDS_TEST_MAIN -o /tmp/sds_test $(FINAL_LIBS)
	@/tmp/sds_test

clean:
	rm -rf *.o

.PHONY: clean
