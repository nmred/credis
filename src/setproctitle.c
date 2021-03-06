#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#if !defined(HAVE_SETPTOCTITLE)
#define HAVE_SETPTOCTITLE (defined __NetBSD || defined __FreeBSD || defined __OpenBSD)
#endif

#if !HAVE_SETPTOCTITLE
#if (defined __linux || defined __APPLE__)

extern char **environ;

static struct {
	const char *arg0;
	char *base, *end;
	char *nul;
	_Bool reset;
	int error;
} SPT;

#ifndef SPT_MIN
#define SPT_MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static inline size_t spt_min(size_t a, size_t b)
{
	return SPT_MIN(a, b);
}

static int spt_clearenv(void)
{
#if __GLIBC__
	clearenv();

	return 0;
#else
	extern char **environ;
	static char **tmp;

	if (!(tmp = malloc(sizeof *tmp))) {
		return errno;
	}

	tmp[0] = NULL;
	environ = tmp;

	return 0;
#endif
}

static int spt_copyenv(char *oldenv[])
{
	extern char **environ;
	char *eq;
	int i, error;

	if (environ != oldenv)
		return 0;

	if ((error = spt_clearenv()))
		goto error;

	for (i = 0; oldenv[i]; i++) {
		if (!(eq = strchr(oldenv[i], '=')))
			continue;
		*eq = '\0';
		error = (0 != setenv(oldenv[i], eq + 1, 1)) ? errno : 0;
		*eq = '=';

		if (error) {
			goto error;
		}
	}

	return 0;

error:
	environ = oldenv;
	return error;
}

static int spt_copyargs(int argc, char *argv[])
{
	char *tmp;
	int i;

	for (i = 1; i < argc || (i >= argc && argv[i]); i++) {
		if (!argv[i])
			continue;

		if (!(tmp = strdup(argv[i])))
			return errno;

		argv[i] = tmp;
	}	

	return 0;
}


void spt_init(int argc, char **argv)
{
	char **envp = environ;
	char *base, *end, *nul, *tmp;
	int i, error;

	if (!(base = argv[0]))
		return;

	nul = &base[strlen(base)];
	end = nul + 1;

	for (i = 0; i < argc || (i <= argc && argv[i]); i++) {
		if (!argv[i] || argv[i] < end) {
			continue;
		}

		end = argv[i] + strlen(argv[i]) + 1;
	}

	for (i = 0; envp[i]; i++) {
		if (envp[i] < end) {
			continue;
		}

		end = envp[i] + strlen(envp[i]) + 1;
	}

	if (!(SPT.arg0 = strdup(argv[0]))) {
		goto syerr;
	}

#if __GLIBC__
	if (!(tmp = strdup(program_invocation_name)))
		goto syerr;

	program_invocation_name = tmp;

	if (!(tmp = strdup(program_invocation_short_name)))
		goto syerr;

	program_invocation_short_name = tmp;

#elif __APPLE__
	if (!(tmp = strdup(getprogname())))
		goto syerr;

	setprogname(tmp);
#endif

	if ((error = spt_copyenv(envp)))
		goto error;
	
	if ((error = spt_copyargs(argc, argv)))
		goto error;

	SPT.nul  = nul;
	SPT.base = base;
	SPT.end  = end;

	return;

syerr:
	error = errno;
error:
	SPT.error = error;
}

#ifndef SPT_MAXTITLE
#define SPT_MAXTITLE 255
#endif

void setproctitle(const char *fmt, ...)
{
	char buf[SPT_MAXTITLE + 1];
	va_list ap;
	char *nul;
	int len, error;

	if (!SPT.base)
		return;

	if (fmt) {
		va_start(ap, fmt);
		len = vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
	} else {
		len = snprintf(buf, sizeof(buf), "%s", SPT.arg0);
	}

	if (len <= 0) {
		error = errno;
		goto error;
	}

	if (!SPT.reset) {
		memset(SPT.base, 0, SPT.end - SPT.base);
		SPT.reset = 1;
	} else {
		memset(SPT.base, 0, spt_min(sizeof(buf), SPT.end - SPT.base));
	}

	len = spt_min(len, spt_min(sizeof(buf), SPT.end - SPT.base) - 1);
	memcpy(SPT.base, buf, len);
	nul = &SPT.base[len];

	if (nul < SPT.nul) {
		*SPT.nul = '.';
	} else if (nul == SPT.nul && &nul[1] < SPT.end) {
		*SPT.nul = ' ';
		*++nul = '\0';
	}

	return;

error:
	SPT.error = error;
}

#endif
#endif
