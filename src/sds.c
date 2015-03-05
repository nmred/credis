#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "sds.h"
#include "zmalloc.h"

sds sdsnewlen(const void *init, size_t initlen)
{
    struct sdshdr *sh;

    if (init) {
        sh = zmalloc(sizeof(struct sdshdr) + initlen + 1);
    } else {
        sh = zcalloc(sizeof(struct sdshdr) + initlen + 1);
    }
    if (sh == NULL) return NULL;
    sh->len  = initlen;
    sh->free = 0;
    if (initlen && init) {
        memcpy(sh->buf, init, initlen);
    }
    sh->buf[initlen] = '\0';
    return (char*)sh->buf;
}

sds sdsempty(void)
{
    return sdsnewlen("", 0);
}

sds sdsnew(const char *init)
{
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

sds sdsdup(const sds s)
{
    return sdsnewlen(s, sdslen(s));
}

void sdsfree(sds s)
{
    if (s == NULL) return;
    zfree(s - sizeof(struct sdshdr));
}

void sdsupdatelen(sds s)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	int reallen = strlen(s);
	sh->free += (sh->len - reallen);
	sh->len = reallen;	
}

void sdsclear(sds s)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	sh->free += sh->len;
	sh->len = 0;
	sh->buf[0] = '\0';	
}

sds sdsMakeRoomFor(sds s, size_t addlen)
{
	struct sdshdr *sh, *newsh;
	size_t free = sdsavail(s);
	size_t len, newlen;

	if (free >= addlen) return s;
	len = sdslen(s);
	sh = (void *)(s - (sizeof(struct sdshdr)));
	newlen = (len + addlen);
	if (newlen < SDS_MAX_PREALLOC) {
		newlen *= 2;
	} else {
		newlen += SDS_MAX_PREALLOC;
	}
	newsh = zrealloc(sh, sizeof(struct sdshdr) + newlen + 1);
	if (newsh == NULL) return NULL;

	newsh->free = newlen - len;
	return newsh->buf;
}

sds sdsRemoveFreeSpace(sds s)
{
	struct sdshdr *sh;

	sh = (void *)(s - (sizeof(struct sdshdr)));
	sh = zrealloc(sh, sizeof(struct sdshdr) + sh->len + 1);
	sh->free = 0;
	return sh->buf;
}

size_t sdsAllocSize(sds s)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));

	return sizeof(*sh) + sh->len + sh->free + 1;
}

void sdsIncrLen(sds s, int incr)
{
	struct sdshdr *sh = (void *)(s - sizeof(struct sdshdr));

	if (incr >= 0) {
		assert(sh->free >= (unsigned int)incr);
	} else {
		assert(sh->len >= (unsigned int)(-incr));
	}

	sh->len += incr;
	sh->free -= incr;

	s[sh->len] = '\0';
}

sds sdsgrowzero(sds s, size_t len)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	size_t totlen, curlen = sh->len;

	if (len <= curlen) return s;
	s = sdsMakeRoomFor(s, len - curlen);
	if (s == NULL) return NULL;

	sh = (void *)(s - (sizeof(struct sdshdr)));
	memset(s + curlen, 0, (len - curlen + 1));
	totlen = sh->len + sh->free;
	sh->len = len;
	sh->free = totlen - sh->len;

	return s;
}

sds sdscatlen(sds s, const void *t, size_t len)
{
	struct sdshdr *sh;
	size_t curlen = sdslen(s);

	s = sdsMakeRoomFor(s, len);
	if (s == NULL) return NULL;
	sh = (void *)(s - sizeof(struct sdshdr));
	memcpy(s + curlen, t, len);
	sh->len = curlen + len;
	sh->free = sh->free - len;
	s[curlen + len] = '\0';
	return s;
}

sds sdscat(sds s, const char *t)
{
	return sdscatlen(s, t, strlen(t));
}

sds sdscatsds(sds s, const sds t)
{
	return sdscatlen(s, t, sdslen(t));	
}

sds sdscpylen(sds s, const char *t, size_t len)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	size_t totlen = sh->free + sh->len;

	if (totlen < len) {
		s = sdsMakeRoomFor(s, len - sh->len);
		if (s == NULL) return NULL;
		sh = (void *)(s - (sizeof(struct sdshdr)));
		totlen = sh->free + sh->len;
	}

	memcpy(s, t, len);
	s[len] = '\0';
	sh->len = len;
	sh->free = totlen - sh->len;
	return s;
}

sds sdscpy(sds s, const char *t)
{
    return sdscpylen(s, t, strlen(t));
}

#define SDS_LISTR_SIZE 21
int sdsll2str(char *s, long long value)
{
	char *p, aux;
	unsigned long long v;
	size_t l;

	v = (value < 0) ? -value : value;
	p = s;
	do {
		*p++ = '0' + (v % 10);
		v /= 10;
	} while (v);
	if (value < 0) *p++ = '-';

	l = p - s;
	*p = '\0';

	p--;
	while (s < p) {
		aux = *s;
		*p = *s;
		*s = aux;
		s++;
		p--;
	}

	return l;
}

int sdsull2str(char *s, unsigned long long v)
{
	char *p, aux;
	size_t l;

	p = s;
	do {
		*p++ = '0' + (v % 10);
		v /= 10;
	} while (v);

	l = p - s;
	*p = '\0';

	p--;
	while (s < p) {
		aux = *s;
		*p = *s;
		*s = aux;
		s++;
		p--;
	}

	return l;
}

sds sdsfromlonglong(long long value)
{
	char buf[SDS_LISTR_SIZE];
	int len = sdsll2str(buf, value);
	return sdsnewlen(buf, len);
}

sds sdscatvprintf(sds s, const char *fmt, va_list ap)
{
	va_list cpy;
	char staticbuf[1024], *buf = staticbuf, *t;
	size_t buflen = strlen(fmt) * 2;

	if (buflen > sizeof(staticbuf)) {
		buf = zmalloc(buflen);
		if (buf == NULL) return NULL;
	} else {
		buflen = sizeof(staticbuf);
	}

	while (1) {
		buf[buflen - 2] = '\0';
		va_copy(cpy, ap);
		vsnprintf(buf, buflen, fmt, cpy);
		va_end(cpy);	
		if (buf[buflen - 2] != '\0') {
			if (buf != staticbuf) zfree(buf);
			buflen *= 2;
			buf = zmalloc(buflen);
			if (buf == NULL) return NULL;
			continue;
		}

		break;
	}

	t = sdscat(s, buf);
	if (buf != staticbuf) zfree(buf);
	return t;
}

sds sdscatprintf(sds s, const char *fmt, ...)
{
	va_list ap;
	char *t;
	va_start(ap, fmt);
	t = sdscatvprintf(s, fmt, ap);
	va_end(ap);
	return t;
}

sds sdscatfmt(sds s, char const *fmt, ...)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	size_t initlen = sdslen(s);
	const char *f = fmt;
	int i;
	va_list ap;
	
	va_start(ap, fmt);
	f = fmt;
	i = initlen;
	while(*f) {
		char next, *str;
		unsigned int l;
		long long num;
		unsigned long long unum;

		if (sh->free == 0) {
			s = sdsMakeRoomFor(s, 1);
			sh = (void *)(s - (sizeof(struct sdshdr)));
		}

		switch(*f) {
		case '%':
			next = *(f + 1);
			f++;
			switch(next) {
			case 's':
			case 'S':
				str = va_arg(ap, char *);
				l = (next == 's') ? strlen(str) : sdslen(str);
				if (sh->free < l) {
					s = sdsMakeRoomFor(s, l);
					sh = (void *)(s - (sizeof(struct sdshdr)));
				}	
				memcpy(s + i, str, l);
				sh->len += l;
				sh->free -= l;
				i += l;
				break;
			case 'i':
			case 'I':
				if (next == 'i') {
					num = va_arg(ap, int);
				} else {
					num = va_arg(ap, long long);
				}
				{
					char buf[SDS_LISTR_SIZE];
					l = sdsll2str(buf, num);
					if (sh->free < l) {
						s = sdsMakeRoomFor(s, l);
						sh = (void *)(s - (sizeof(struct sdshdr)));
					}
					memcpy(s + i, buf, l);
					sh->len += l;
					sh->free -= l;
					i += l;
				}
				break;
			case 'u':
			case 'U':
				if (next == 'u') {
					unum = va_arg(ap, unsigned int);
				} else {
					unum = va_arg(ap, unsigned long long);
				}
				{
					char buf[SDS_LISTR_SIZE];
					l = sdsull2str(buf, unum);
					if (sh->free < l) {
						s = sdsMakeRoomFor(s, l);
						sh = (void *)(s - (sizeof(struct sdshdr)));
					}
					memcpy(s + i, buf, l);
					sh->len += l;
					sh->free -= l;
					i += l;
				}
				break;
			default:
				s[i++] = next;
				sh->len += 1;
				sh->free -= 1;
				break;
			}
			break;
		default:
			s[i++] = *f;
			sh->len += 1;
			sh->free -= 1;
			break;
		}
		f++;
	}
	va_end(ap);

	s[i] = '\0';
	return s;
}

sds sdstrim(sds s, const char *cset)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	char *start, *end, *sp, *ep;
	size_t len;
	
	sp = start = s;
	ep = end = s + sdslen(s) - 1;	
	while (sp <= end && strchr(cset, *sp)) sp++;
	while (ep > sp && strchr(cset, *ep)) ep--;
	len = (sp > ep) ? 0 : ((ep - sp) + 1);
	if (sh->buf != sp) memmove(sh->buf, sp, len);
	sh->buf[len] = '\0';
	sh->free = sh->free + (sh->len - len);
	sh->len = len;
	return s;
}

void sdsrange(sds s, int start, int end)
{
	struct sdshdr *sh = (void *)(s - (sizeof(struct sdshdr)));
	size_t newlen, len = sdslen(s);	

	if (len == 0) return;
	if (start < 0) {
		start = len + start;
		if (start < 0) start = 0;
	}
	if (end < 0) {
		end = len + end;
		if (end < 0) end = 0;
	}

	newlen = (start > end) ? 0 : (end - start) + 1;
	if (newlen != 0) {
		if (start >= (signed)len) {
			newlen = 0;
		} else if (end >= (signed)len) {
			end = len - 1;
			newlen = (start > end) ? 0 : (end - start) + 1;
		}
	} else {
		start = 0;
	}

	if (start && newlen) memmove(sh->buf, sh->buf + start, newlen);
	sh->buf[newlen] = 0;
	sh->free = sh->free + (sh->len - newlen);
	sh->len = newlen;
}

void sdstolower(sds s)
{
	int len = sdslen(s), j;
	for (j = 0; j < len; j++) s[j] = tolower(s[j]);
}

void sdstoupper(sds s)
{
	int len = sdslen(s), j;
	for (j = 0; j < len; j++) s[j] = toupper(s[j]);
}

int sdscmp(const sds s1, const sds s2)
{
	size_t l1, l2, minlen;
	int cmp;

	l1 = sdslen(s1);
	l2 = sdslen(s2);
	minlen = (l1 < l2) ? l1 : l2;
	cmp = memcmp(s1, s2, minlen);
	if (cmp == 0) return l1 - l2;
	return cmp;
}

sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count)
{
	int elements = 0, slots = 5, start = 0, j;
	sds *tokens;

	if (seplen < 1 || len < 0) return NULL;
	tokens = zmalloc(sizeof(sds) * slots);
	if (tokens == NULL) return NULL;

	if (len == 0) {
		*count = 0;
		return tokens;
	}

	for (j = 0; j < (len - (seplen - 1)); j++) {
		if (slots < elements + 2) {
			sds *newtokens;

			slots *= 2;
			newtokens = zrealloc(tokens, sizeof(sds) * slots);
			if (newtokens == NULL) goto cleanup;
			tokens = newtokens;
		}

		if ((seplen == 1 && *(s + j) == sep[0]) || (memcmp(s + j, sep, seplen) == 0)) {
			tokens[elements] = sdsnewlen(s + start, j - start);
			if (tokens[elements] == NULL) goto cleanup;
			elements++;
			start = j + seplen;
			j = j + seplen - 1; // 跳过分隔符
		}
	}
	tokens[elements] = sdsnewlen(s + start, len - start);
	elements++;
	*count = elements;
	return tokens;
cleanup:
	{
		int i;
		for (i = 0; i < elements; i++) sdsfree(tokens[i]);
		zfree(tokens);
		*count = 0;
		return NULL;
	}
}

void sdsfreesplitres(sds *tokens, int count)
{
	if (!tokens) return;
	while(count--) {
		sdsfree(tokens[count]);
	}
	zfree(tokens);
}

sds sdscatrepr(sds s, const char *p, size_t len)
{
	s = sdscatlen(s, "\"", 1);
	while(len--) {
		switch (*p) {
		case '\\':
		case '"':
			s = sdscatprintf(s, "\\%c", *p);
			break;
		case '\n': s = sdscatlen(s, "\\n", 2); break;
		case '\r': s = sdscatlen(s,"\\r",2); break;
		case '\t': s = sdscatlen(s,"\\t",2); break;
		case '\a': s = sdscatlen(s,"\\a",2); break;
		case '\b': s = sdscatlen(s,"\\b",2); break;
		default:
		   if (isprint(*p))
			   s = sdscatprintf(s,"%c",*p);
		   else
			   s = sdscatprintf(s,"\\x%02x",(unsigned char)*p);
		   break;
		}
		p++;
	}

	return sdscatlen(s, "\"", 1);
}

int is_hex_digit(char c) 
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int hex_digit_to_int(char c)
{
	switch(c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return 0;		
	}
}

sds *sdssplitargs(const char *line, int *argc)
{
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while(1) {
        /* skip blanks */
        while(*p && isspace(*p)) p++;
        if (*p) {
            /* get a token */
            int inq=0;  /* set to 1 if we are in "quotes" */
            int insq=0; /* set to 1 if we are in 'single quotes' */
            int done=0;

            if (current == NULL) current = sdsempty();
            while(!done) {
                if (inq) {
                    if (*p == '\\' && *(p+1) == 'x' &&
                                             is_hex_digit(*(p+2)) &&
                                             is_hex_digit(*(p+3)))
                    {
                        unsigned char byte;

                        byte = (hex_digit_to_int(*(p+2))*16)+
                                hex_digit_to_int(*(p+3));
                        current = sdscatlen(current,(char*)&byte,1);
                        p += 3;
                    } else if (*p == '\\' && *(p+1)) {
                        char c;

                        p++;
                        switch(*p) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'b': c = '\b'; break;
                        case 'a': c = '\a'; break;
                        default: c = *p; break;
                        }
                        current = sdscatlen(current,&c,1);
                    } else if (*p == '"') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p+1) && !isspace(*(p+1))) goto err;
                        done=1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdscatlen(current,p,1);
                    }
                } else if (insq) {
                    if (*p == '\\' && *(p+1) == '\'') {
                        p++;
                        current = sdscatlen(current,"'",1);
                    } else if (*p == '\'') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p+1) && !isspace(*(p+1))) goto err;
                        done=1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdscatlen(current,p,1);
                    }
                } else {
                    switch(*p) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done=1;
                        break;
                    case '"':
                        inq=1;
                        break;
                    case '\'':
                        insq=1;
                        break;
                    default:
                        current = sdscatlen(current,p,1);
                        break;
                    }
                }
                if (*p) p++;
            }
            /* add the token to the vector */
            vector = zrealloc(vector,((*argc)+1)*sizeof(char*));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        } else {
            /* Even on empty input string return something not NULL. */
            if (vector == NULL) vector = zmalloc(sizeof(void*));
            return vector;
        }
    }

err:
    while((*argc)--)
        sdsfree(vector[*argc]);
    zfree(vector);
    if (current) sdsfree(current);
    *argc = 0;
    return NULL;
}

sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen)
{
	size_t j, i, l = sdslen(s);
	
	for (j = 0; j < l; j++) {
		for (i = 0; i < setlen; i++) {
			if (s[j] == from[i]) {
				s[j] = to[i];
				break;
			}
		}
	}	

	return s;
}

sds sdsjoin(char **argv, int argc, char *sep)
{
	sds join = sdsempty();
	int j;

	for (j = 0; j < argc; j++) {
		join = sdscat(join, argv[j]);
		if (j != argc - 1) join = sdscat(join, sep);
	}

	return join;
}

#if defined(REDIS_TEST) || defined(SDS_TEST_MAIN)
#include <stdio.h>
#include <limits.h>
#include "testhelp.h"

#define UNUSED(x) (void)(x)

int sdsTest(int argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);
	{
		struct sdshdr *sh;
		sds x = sdsnew("foo"), y;	

		test_cond("Create a string and obtain the length",
			sdslen(x) == 3 && memcmp(x, "foo\0", 4) == 0)
		sdsfree(x);
		x = sdsnewlen("foo", 2);
		test_cond("Create a string with specified length",
			sdslen(x) == 2 && memcmp(x, "fo\0", 3) == 0)

		x = sdscat(x, "bar");
		test_cond("String concatenation",
			sdslen(x) == 5 && memcmp(x, "fobar\0", 6) == 0)

		x = sdscpy(x, "a");
		test_cond("sdscpy() against an originally longer string",
			sdslen(x) == 1 && memcmp(x, "a\0", 2) == 0)

		x = sdscpy(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
		test_cond("sdscpy() against an originally shorter string",
			sdslen(x) == 33 && memcmp(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0", 34) == 0)
		sdsfree(x);
		x = sdscatprintf(sdsempty(), "%d", 123);
		test_cond("sdscatprintf() seems working in the base case",
			sdslen(x) == 3 && memcmp(x, "123\0", 4) == 0)
		
		sdsfree(x);
		x = sdsnew("--");
		x = sdscatfmt(x, "Hello %s World %I, %I--", "Hi!", LLONG_MIN, LLONG_MAX);
		test_cond("sdscatfmt() seems working in the base case",
			sdslen(x) == 61 &&
			memcmp(x, "--Hello Hi! World 80857745866854775808, 7085774586854775807--", 60) == 0)

		sdsfree(x);
		x = sdsnew("--");
		x = sdscatfmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
		test_cond("sdscatfmt() seems working with usigned numbers",
			sdslen(x) == 35 &&
			memcmp(x, "--5927667295,51615590733709551615--", 35) == 0)
			
		sdsfree(x);
		x = sdsnew(" x ");
		sdstrim(x, " x");
		test_cond("sdstrim() works when all chars match",
			sdslen(x) == 0)

		sdsfree(x);
		x = sdsnew(" x ");
		sdstrim(x, " ");
		test_cond("sdstrim() works when a signle char remains",
			sdslen(x) == 1 && x[0] == 'x')

		sdsfree(x);
		x = sdsnew("xxciaoyyy");
		sdstrim(x, "xy");
		test_cond("sdstrim() correctly trims characters",
			sdslen(x) == 4 && memcmp(x, "ciao\0", 5) == 0);

		y = sdsdup(x);
		sdsrange(y, 1, 1);
		test_cond("sdsrange(..., 1, 1)",
			sdslen(y) == 1 && memcmp(y, "i\0", 2) == 0)

		sdsfree(y);
		y = sdsdup(x);
		sdsrange(y, 1, -1);
		test_cond("sdsrange(..., 1, -1)",
			sdslen(y) == 3 && memcmp(y, "iao\0", 4) == 0)

		sdsfree(y);
		y = sdsdup(x);
		sdsrange(y, 2, 1);
		test_cond("sdsrange(..., 2, 1)",
			sdslen(y) == 0 && memcmp(y, "\0", 1) == 0)

		sdsfree(y);
		y = sdsdup(x);
		sdsrange(y, -2, -1);
		test_cond("sdsrange(..., -2, -1)",
			sdslen(y) == 2 && memcmp(y, "ao\0", 3) == 0)

		sdsfree(y);
		y = sdsdup(x);
		sdsrange(y, 1, 100);
		test_cond("sdsrange(..., 1, 100)",
			sdslen(y) == 3 && memcmp(y, "iao\0", 4) == 0)

		sdsfree(y);
		y = sdsdup(x);
		sdsrange(y, 100, 100);
		test_cond("sdsrange(..., 100, 100)",
			sdslen(y) == 0 && memcmp(y, "\0", 1) == 0)

		sdsfree(x);
		sdsfree(y);
		x = sdsnew("foo");
		y = sdsnew("foa");
		test_cond("sdscmp(foo, foa)", sdscmp(x, y) > 0)

		sdsfree(x);
		sdsfree(y);
		x = sdsnew("bar");
		y = sdsnew("bar");
		test_cond("sdscmp(foo, foa)", sdscmp(x, y) == 0)

		sdsfree(x);
		sdsfree(y);
		x = sdsnew("aar");
		y = sdsnew("bar");
		test_cond("sdscmp(aar, bar)", sdscmp(x, y) < 0)

		sdsfree(y);
		sdsfree(x);
		x = sdsnewlen("\a\n\0foo\r", 7);
		y = sdscatrepr(sdsempty(), x, sdslen(x));
		test_cond("sdscatrepr(...data...)",
			memcmp(y, "\"\\a\\n\\x00foo\\r\"", 15) == 0)

		{
			unsigned int oldfree;

			x = sdsnew("0");	
			sh = (void *)(x - sizeof(struct sdshdr));
			test_cond("sdsnew() free/len buffers",
				sh->len == 1 && sh->free == 0)

			x = sdsMakeRoomFor(x, 1);
			sh = (void *)(x - sizeof(struct sdshdr));
			test_cond("sdsMakeRoomFor()", sh->len == 1 && sh->free > 0)
			oldfree = sh->free;
			x[1] = '1';
			sdsIncrLen(x, 1);
			test_cond("sdsIncrLen() -- content", x[0] == '0' && x[1] == '1')
			test_cond("sdsIncrLen() -- len", sh->len == 2);
			test_cond("sdsIncrLen() -- free", sh->free == oldfree - 1);
		}
	}

	test_report()
	return 0;
}

#endif

#ifdef SDS_TEST_MAIN
int main(int argc, char **argv)
{
	return sdsTest(argc, argv);
}
#endif
