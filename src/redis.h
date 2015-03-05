#ifndef __REDIS_H
#define __REDIS_H

#include "config.h"
#include "fmacroc.h"

#include "zmalloc.h"
#include "release.h"
#include "version.h"
#include "dict.h"
#include "sds.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>

/* Error codes */
#define REDIS_OK 0
#define REDIS_ERR -1

/* 日志等级 */
#define REDIS_DEBUG 0
#define REDIS_VERBOSE 1
#define REDIS_NOTICE 2
#define REDIS_WARNING 3
#define REDIS_LOG_RAW (1<<10)
#define REDIS_DEFAULT_VERBOSITY REDIS_NOTICE

/* 超出最大内存的处理策略 */
#define REDIS_MAXMEMORY_VOLATILE_LRU 0
#define REDIS_MAXMEMORY_VOLATILE_TTL 1
#define REDIS_MAXMEMORY_VOLATILE_RANDOM 2
#define REDIS_MAXMEMORY_ALLKEYS_LRU 3
#define REDIS_MAXMEMORY_ALLKEYS_RANDOM 4
#define REDIS_MAXMEMORY_NO_EVICTION 5
#define REDIS_DEFAULT_MAXMEMORY_POLITY REDIS_MAXMEMORY_NO_EVICTION

/* server 端配置 */
#define REDIS_CONFIGLINE_MAX 1024
#define REDIS_MAX_LOGMSG_LEN 1024

#define REDIS_DEFAULT_LOGFILE ""
#define REDIS_DEFAULT_SYSLOG_ENABLED 0
#define REDIS_DEFAULT_SYSLOG_IDENT "redis"
#define REDIS_DEFAULT_MAXMEMORY_SMAPLES 5
#define REDIS_BINDADDR_MAX 16

/* 主从同步的状态 */
#define REDIS_REPL_NONE 0
#define REDIS_REPL_CONNECT 1
#define REDIS_REPL_CONNECTING 2
#define REDIS_REPL_RECEIVE_PONG 3
#define REDIS_REPL_TRANSFER 4
#define REDIS_REPL_CONNECTED 5

/* Debugging */
#define redisAssert(_e) ((_e) ? (void)0 : (_redisAssert(#_e, __FILE__, __LINE__), _exit(1)))
#define redisPanic(_e) _redisPanic(#_e, __FILE__, __LINE__), _exit(1)

struct saveparam {
	// 多少秒之内
	time_t seconds;

	// 发生多少次修改
	int changes;
};

struct redisServer {
	/* General */
	pid_t pid;

	// 配置文件的绝对路径
	char *configFile;
	
	// serverCron 每秒执行的次数
	int hz;

	// 数据库
	
	// 命令表
	dict *commands;
	dict *orig_commands;
	
	// 事件状态
	
	/* Networking */
	
	int port;

	// tcp backlog 长度
	int tcp_backlog;

	// 绑定地址
	char *bindaddr[REDIS_BINDADDR_MAX];

	// 地址数量
	int bindaddr_count;

	// Unix 套接字
	char *unixsocket;
	mode_t unixsocketperm;
	
	int sentinel_mode;

	/* logging */
	char *logfile;
	int syslog_enabled;
	char *syslog_ident;
	int syslog_facility;

	/* configuration */

	// 日志可见等级
	int verbosity;
	
	// 客户端最大空闲时间
	int maxidletime;

	// 是否开启 SO_KEEPALIVE 选项
	int tcpkeepalive;

	int dbnum;

	/* AOF 持久化 */
	struct saveparam *saveparams;
	int saveparamslen;	

	/* 主从相关配置(主)*/
	int repl_ping_slave_period;
	int repl_diskless_sync;
	int repl_diskless_sync_delay;

	/* 主从相关配置(从) */
	char *masterauth;
	char *masterhost;
	int masterport;
	int repl_state;
	int repl_timeout;
	int repl_disable_tcp_nodelay;
	int repl_serve_stale_data;

	/* Limits */
	unsigned int maxclients;
	unsigned long long maxmemory;
	int maxmemory_policy;
	int maxmemory_samples;

	/*debug assert*/
	char *assert_failed;
	char *assert_file;
	int assert_line;
	int bug_report_start;

};


extern struct redisServer server;

/* Core functions */
#ifdef __GNUC__
void redisLog(int level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
#else
void redisLog(int level, const char *fmt, ...);
#endif
void redisLogRaw(int level, const char *msg);

/* Git SHA1 */
char *redisGitSHA1(void);
char *redisGitDirty(void);
uint64_t redisBuildId(void);

/*Debugging stuff*/
void _redisAssert(char *estr, char *file, int line);
void _redisPanic(char *msg, char *file, int line);
void bugReportStart(void);

/* Configuration */
void loadServerConfig(char *filename, char *options);
void appendServerSaveParams(time_t seconds, int changes);
void resetServerSaveParams(void);

#endif
