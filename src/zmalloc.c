#include <stdio.h>
#include <stdlib.h>

void zlibc_free(void *ptr) {
	free(ptr);
}

#include <string.h>
#include <pthread.h>
#include "config.h"
#include "zmalloc.h"

#ifdef HAVE_MALLOC_SIZE
#define PREFIX_SIZE (0)
#else
#define PREFIX_SIZE (sizeof(size_t))
#endif

#if defined(USE_JEMALLOC)
#define malloc(size) je_malloc(size)
#define calloc(count, size) je_calloc(count, size)
#define realloc(ptr, size) je_realloc(ptr, size)
#define free(pre) je_free(ptr)
#endif

#if defined(__ATOMIC_RELAXED)
#define update_zmalloc_stat_add(_n) __atomic_add_fetch(&used_memory, (_n), __ATOMIC_RELAXED)
#define update_zmalloc_stat_sub(_n) __atomic_sub_fetch(&used_memory, (_n), __ATOMIC_RELAXED)
#elif defined(HAVA_ATOMIC)
#define update_zmalloc_stat_add(_n) __sysc_add_and_fetch(&used_memory, (_n))
#define update_zmalloc_stat_sub(_n) __sysc_sub_and_fetch(&used_memory, (_n))
#else
#define update_zmalloc_stat_add(__n) do { \
	pthread_mutex_lock(&used_memory_mutex); \
	used_memory += (__n); \
	pthread_mutex_unlock(&used_memory_mutex); \
} while(0)

#define update_zmalloc_stat_sub(__n) do { \
	pthread_mutex_lock(&used_memory_mutex); \
	used_memory -= (__n); \
	pthread_mutex_unlock(&used_memory_mutex); \
} while(0)
#endif

#define update_zmalloc_stat_alloc(__n) do { \
	size_t _n = (__n); \
	if (_n & (sizeof(long) - 1)) _n += sizeof(long) - (_n & (sizeof(long) - 1)); \
	if (zmalloc_thread_safe) { \
		update_zmalloc_stat_add(_n); \
	} else { \
		used_memory += _n; \
	} \
} while(0)

#define update_zmalloc_stat_free(__n) do { \
	size_t _n = (__n); \
	if (_n & (sizeof(long) - 1)) _n += sizeof(long) - (_n & (sizeof(long) - 1)); \
	if (zmalloc_thread_safe) { \
		update_zmalloc_stat_sub(_n); \
	} else { \
		used_memory -= _n; \
	} \
} while(0)

static size_t used_memory = 0;
static int zmalloc_thread_safe = 0;
pthread_mutex_t used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

static void zmalloc_default_oom(size_t size) {
	fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n", size);
	fflush(stderr);
	abort();
}

static void (*zmalloc_oom_handler)(size_t) = zmalloc_default_oom;

void *zmalloc(size_t size) {
	void *ptr = malloc(size + PREFIX_SIZE);

	if (!ptr) zmalloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE
	update_zmalloc_stat_alloc(zmalloc_size(ptr));
	return ptr;
#else
	*((size_t*)ptr) = size;
	update_zmalloc_stat_alloc(size + PREFIX_SIZE);
	return (char*)ptr + PREFIX_SIZE;
#endif
}

void *zcalloc(size_t size) {
	void *ptr = calloc(1, size + PREFIX_SIZE);

	if (!ptr) zmalloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE
	update_zmalloc_stat_alloc(zmalloc_size(ptr));
	return ptr;
#else
	*((size_t*)ptr) = size;
	update_zmalloc_stat_alloc(size + PREFIX_SIZE);
	return (char*)ptr + PREFIX_SIZE;
#endif
}

void *zrealloc(void *ptr, size_t size) {
#ifndef HAVE_MALLOC_SIZE
	void *realptr;
#endif
	size_t oldsize;
	void *newptr;

	if (ptr == NULL) return zmalloc(size);
#ifdef HAVE_MALLOC_SIZE
	oldsize = zmalloc_size(ptr);
	newptr = realloc(ptr, size);
	if (!newptr) zmalloc_oom_handler(size);
	update_zmalloc_stat_free(oldsize);
	update_zmalloc_stat_alloc(zmalloc_size(newptr));
	return newptr;
#else
	realptr = (char*)ptr - PREFIX_SIZE;
	oldsize = *((size_t*)realptr);
	newptr = realloc(realptr, size + PREFIX_SIZE);
	if (!newptr) zmalloc_oom_handler(size);

	*((size_t*)newptr) = size;
	update_zmalloc_stat_free(oldsize);
	update_zmalloc_stat_alloc(size);
	return (char*)newptr + PREFIX_SIZE;
#endif
}


#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void *ptr) {
	void *realptr = (char*)ptr - PREFIX_SIZE;
	size_t size = *((size_t*)realptr);
	if (size & (sizeof(long) - 1)) size += sizeof(long) - (size & (sizeof(long) - 1));
	return size + PREFIX_SIZE;
}
#endif

void zfree(void *ptr) {
#ifndef HAVE_MALLOC_SIZE
	void *realptr;
	size_t oldsize;
#endif
	if (ptr == NULL) return;
#ifdef HAVE_MALLOC_SIZE
	update_zmalloc_stat_free(zmalloc_size(ptr));
	free(ptr);
#else
	realptr = (char*)ptr - PREFIX_SIZE;
	oldsize = *((size_t*)realptr);
	update_zmalloc_stat_free(oldsize + PREFIX_SIZE);
	free(realptr);
#endif
}

char *zstrdup(const char *s) {
	size_t l = strlen(s) + 1;
	char *p = zmalloc(l);

	memcpy(p, s, l);
	return p;
}

size_t zmalloc_used_memory(void) {
	size_t um;
	if (zmalloc_thread_safe) {
#if defined(__ATOMIC_RELAXED) || defined(HAVA_ATOMIC)
		um = update_zmalloc_stat_add(0);
#else
		pthread_mutex_lock(&used_memory_mutex);
		um = used_memory;
		pthread_mutex_unlock(&used_memory_mutex);
#endif
	} else {
		um = used_memory;
	}

	return um;
}

void zmalloc_enable_thread_safeness(void) {
	zmalloc_thread_safe = 1;
}

void zmalloc_set_oom_handler(void (*oom_handler)(size_t)) {
	zmalloc_oom_handler = oom_handler;
}

#if defined(HAVA_PROC_STAT)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

size_t zmalloc_get_rss(void) {
	int page = sysconf(_SC_PAGESIZE);
	size_t rss;
	char buf[4096];
	char filename[256];
	int fd, count;
	char *p, *x;

	snprintf(filename, 256, "/proc/%d/stat", getpid());
	if ((fd = open(filename, O_RDONLY)) == -1) return 0;
	if (read(fd, buf, 4096) <= 0) {
		close(fd);
		return 0;
	}
	close(fd);

	p = buf;
	count = 23;
	while(p && count--) {
		p = strchr(p, ' ');
		if (p) p++;
	}
	if (!p) return 0;
	x = strchr(p, ' ');
	if (!x) return 0;
	*x = '\0';

	rss = strtoll(p, NULL, 10);
	rss *= page;
	return rss;
}

#elif defined(HAVA_TASKINFO)
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/task.h>
#include <mach/mach_init.h>

size_t zmalloc_get_rss(void) {
	task_t task = MACH_PORT_NULL;
	struct task_basic_info t_info;
	mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

	if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS)
		return 0;
	task_info(taskm TASK_BASIC_INFO, (task_info_t) & t_info, &t_info_count);

	return t_info.resident_size;
}

#else
size_t zmalloc_get_rss(void) {
	return zmalloc_used_memory();
}
#endif

float zmalloc_get_fragmentation_ratio(size_t rss) {
	return (float)rss / zmalloc_used_memory();
}

#if defined(HAVA_PROC_SMAPS)
size_t zmalloc_get_smap_bytes_by_field(char *field) {
	char line[1024];
	size_t bytes = 0;
	FILE *fp = fopen("/proc/self/smaps", "r");
	int flen = strlen(field);
	
	if (!fp) return 0;
	while(fgets(line, sizeof(line), fp) != NULL) {
		if (strncmp(line, field, flen) == 0) {
			char *p = strchr(line, 'k');
			if (p) {
				*p = '\0';
				bytes += strtol(line + flen, NULL, 10) * 1024;
			}
		}
	}

	fclose(fp);
	return bytes;
}

#else
size_t zmalloc_get_smap_bytes_by_field(char *field) {
	((void)field);
	return 0;	
}
#endif

size_t zmalloc_get_private_dirty(void) {
	return zmalloc_get_smap_bytes_by_field("Private_Dirty:");
}

size_t zmalloc_get_memory_size(void) {
#if defined(__unix__) || defined(__unix) || defined(unix) || \
	(defined(__APPLE__) && defined(__MACH__))
#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
	mib[1] = HW_MEMSIZE;
#elif defined(HW_PHYSMEM64)
	mib[1] = HW_PHYSMEM64;
#endif
	int64_t size = 0;
	size_t len = sizeof(size);
	if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) {
		return (size_t)size;
	}
	return 0L;
#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
	return (size_t)sysconf(_SC_PHYS_PAGES) * (size_t)sysconf(_SC_PAGESIZE);
#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_REALMEM)
	mib[1] = HW_REALMEM;
#elif defined(HW_PHYSMEM)
	mib[1] = HW_PHYSMEM;
#endif
	unsigned int size = 0;
	size_t len = sizeof(size);
	if (sysctl(mib, 2, &size, &len, NULL, 0) == 0)
		return (size_t)size;
	return 0L;
#endif

#else
	return 0L;
#endif
}

