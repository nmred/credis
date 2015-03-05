# redis 依赖 makefile

CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

default:
	@echo "Explict target required"

.PHONY: default

distclean:
	-(cd jemalloc && [ -f Makefile ] && $(MAKE) distclean) > /dev/null || true

.PHONY: distclean

JEMALLOC_CFLAGS= -std=gnu99 -Wall -pipe -g3 -O3 -funroll-loops $(CFLAGS)
JEMALLOC_LDFLAGS= $(LDFLAGS)

jemalloc:
	@printf '%b %b \n' $(MAKECOLOR)MAKE$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR)
	cd jemalloc && ./configure --with-jemalloc-prefix=je_ --enable-cc-silence CFLAGS="$(JEMALLOC_CFLAGS)" LDFLAGS="$(JEMALLOC_LDFLAGS)"
	cd jemalloc && $(MAKE) CFLAGS="$(JEMALLOC_CFLAGS)" LDFLAGS="$(JEMALLOC_LDFLAGS)" lib/libjemalloc.a

.PHONY: jemalloc
