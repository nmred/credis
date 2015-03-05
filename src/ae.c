#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "ae.h"
#include "zmalloc.h"
#include "config.h"

#ifdef HAVE_EVPORT
#include "ae_evport.c"
#else
	#ifdef HAVE_EPOLL
	#include "ae_epoll.c"
	#else
		#ifdef HAVE_KQUEUE
		#include "ae_kqueue.c"
		#else
		#include "ae_select.c"
		#endif
	#endif
#endif


