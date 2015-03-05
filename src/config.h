#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __linux__
#define HAVA_PROC_STAT 1
#define HAVA_PROC_MAPS 1
#define HAVA_PROC_SMAPS 1
#endif

#if defined(__APPLE__)
#define HAVE_TASKINFO 1
#endif

// test backtrace()
#if defined(__APPLE__) || (defined(__linux__) && defined(__GLIBC__))
#define HAVE_BACKTRACE 1
#endif

#ifdef __linux__
#define	HAVE_MSG_NOSIGNAL 1
#endif

#ifdef __linux__
#define HAVE_EPOLL 1
#endif

#if (defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6)) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define HAVE_KQUEUE 1
#endif

#if (defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__)
#define USE_SETPROCTITLE
#endif

#if (defined(__linux) || defined __APPLE__)
#define USE_SETPROCTITLE
#define INI_SETPROCTITLE_REPLACEMENT
void spt_init(int argc, char *argv[]);
void setproctitle(const char *fmt, ...);
#endif

#if (__i386 || __amd64 || __powerpc__) && __GNUC__
#define GNUC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if defined(__clang__)
#define HAVE_ATOMIC
#endif
#if (defined(__GLIBC__) && defined(__GLIBC_PREREQ))
#if (GNUC_VERSION >= 40100 && __GLIBC_PREREQ(2, 6))
#define HAVE_ATOMIC
#endif
#endif
#endif

#endif
