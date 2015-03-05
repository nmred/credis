#include "fmacroc.h"
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#include "util.h"

long long memtoll(const char *p, int *err)
{
	const char *u;
	char buf[128];
	long mul;
	long long val;
	unsigned int digits;

	if (err) *err = 0;
	u = p;
	if (*u == '-') u++;
	while(*u && isdigit(*u)) u++;
	if (*u == '\0' || !strcasecmp(u, "b")) {
		mul = 1;
	} else if (!strcasecmp(u, "k")) {
		mul = 1000;
	} else if (!strcasecmp(u, "kb")) {
		mul = 1024;
	} else if (!strcasecmp(u, "m")) {
		mul = 1000 * 1000;
	} else if (!strcasecmp(u, "mb")) {
		mul = 1024 * 1024;
	} else if (!strcasecmp(u, "g")) {
		mul = 1000 * 1000 * 1000;
	} else if (!strcasecmp(u, "gb")) {
		mul = 1024 * 1024 * 1024;
	} else {
		if (err) *err = 1;
		mul = 1;
	}

	digits = u - p;
	if (digits >= sizeof(buf)) {
		if (err) *err = 1;
		return LLONG_MAX;
	}
	memcpy(buf, p, digits);
	buf[digits] = '\0';
	val = strtoll(buf, NULL, 10);
	return val * mul;
}

sds getAbsolutePath(char *filename)
{
	char cwd[1024];
	sds abspath;
	sds relpath = sdsnew(filename);

	relpath = sdstrim(relpath, " \r\n\t");
	if (relpath[0] == '/') return relpath;

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		sdsfree(relpath);
		return NULL;
	}
	abspath = sdsnew(cwd);
	if (sdslen(abspath) && abspath[sdslen(abspath) - 1] != '/') {
		abspath = sdscat(abspath, "/");
	}

	while (sdslen(relpath) >= 3 &&
		   relpath[0] == '.' && relpath[1] == '.' && relpath[2] == '/') {
		sdsrange(relpath, 3, -1);
		if (sdslen(abspath) > 1) {
			char *p = abspath + sdslen(abspath) - 2;
			int trimlen = 1;
			while (*p != '/') {
				p--;
				trimlen++;
			}

			sdsrange(abspath, 0, -(trimlen + 1));
		}
	}

	abspath = sdscatsds(abspath, relpath);
	sdsfree(relpath);
	return abspath;
}

int pathIsBaseName(char *path)
{
	return strchr(path, '/') == NULL && strchr(path, '\\') == NULL;
}
