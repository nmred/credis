#ifndef __SDS_H
#define __SDS_H

#define SDS_MAX_PREALLOC (1024 * 1024)

#include <sys/types.h>
#include <stdarg.h>

typedef char *sds;

struct sdshdr {
	unsigned int len;
	unsigned int free;
	char buf[];
};

static inline size_t sdslen(const sds s)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	return sh->len;
}

static inline size_t sdsavail(const sds s)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	return sh->free;
}

sds sdsnewlen(const void *init, size_t initlen);
sds sdsnew(const char *init);
sds sdsempty(void);
sds sdsdup(const sds s);
void sdsfree(sds s);
size_t sdslen(const sds s);
size_t sdsavail(const sds s);
void sdsupdatelen(sds s);
void sdsclear(sds s);
sds sdsMakeRoomFor(sds s, size_t addlen);
sds sdsRemoveFreeSpace(sds s);
size_t sdsAllocSize(sds s);
void sdsIncrLen(sds s, int incr);
sds sdsgrowzero(sds s, size_t len);
sds sdscatlen(sds s, const void *t, size_t len);
sds sdscat(sds s, const char *t);
sds sdscatsds(sds s, const sds t);
sds sdscpylen(sds s, const char *t, size_t len);
sds sdscpy(sds s, const char *t);
int sdsll2str(char *s, long long value);
int sdsull2str(char *s, unsigned long long v);
sds sdsfromlonglong(long long value);
sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
sds sdscatprintf(sds s, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char *fmt, ...);
#endif
sds sdscatfmt(sds s, char const *fmt, ...);
sds sdstrim(sds s, const char *cset);
void sdsrange(sds s, int start, int end);
void sdstolower(sds s);
void sdstoupper(sds s);
int sdscmp(const sds s1, const sds s2);
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);
void sdsfreesplitres(sds *tokens, int count);
sds sdscatrepr(sds s, const char *p, size_t len);
sds *sdssplitargs(const char *line, int *argc);
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);
sds sdsjoin(char **argv, int argc, char *sep);

#endif
