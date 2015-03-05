#include "redis.h"

#include <locale.h>

struct redisServer server;

void redisOutOfMemoryHandler(size_t allocation_size)
{
	redisLog(REDIS_WARNING, "Out Of Memory allocating %zu bytes!", allocation_size);
	redisPanic("Redis aborting for OUT OF MEMORY");
}

// 检查redis是否是哨兵模式运行
int checkForSentinelMode(int argc, char **argv)
{
	int j;

	if (strstr(argv[0], "redis-sentinel") != NULL) return 1;
	for (j = 1; j < argc; j++) {
		if (!strcmp(argv[j], "--sentinel")) return 1;
	}

	return 0;
}

void initServerConfig(void)
{
	server.verbosity = REDIS_DEFAULT_VERBOSITY;
	server.logfile = zstrdup(REDIS_DEFAULT_LOGFILE);
	server.syslog_enabled = REDIS_DEFAULT_SYSLOG_ENABLED;
	server.syslog_ident = zstrdup(REDIS_DEFAULT_SYSLOG_IDENT);
	server.syslog_facility = LOG_LOCAL0;
	/* 主从相关配置 */
	server.masterhost = NULL;

	/* Debugging */
	server.assert_failed = "<no assertion failed>";
	server.assert_file = "<no file>";
	server.assert_line = 0;
	server.bug_report_start = 0;
}

void redisLogRaw(int level, const char *msg)
{
	const int syslogLevelMap[] = { LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING };
	const char *c = ".-*#";
	FILE *fp;
	char buf[64];
	int rawmode = (level & REDIS_LOG_RAW);
	int log_to_stdout = (server.logfile[0] == '\0');

	level &= 0xff;
	if (level < server.verbosity) return;

	fp = log_to_stdout ? stdout : fopen(server.logfile, "a");
	if (!fp) return;

	if (rawmode) {
		fprintf(fp, "%s", msg);
	} else {		
		int off;
		struct timeval tv;
		int role_char;
		pid_t pid = getpid();

		gettimeofday(&tv, NULL);
		off = strftime(buf, sizeof(buf), "%d %d %H:%M:%S.", localtime(&tv.tv_sec));
		snprintf(buf + off, sizeof(buf) - off, "%03d", (int)tv.tv_usec / 1000);
		if (server.sentinel_mode) {
			role_char = 'X';
		} else if (pid != server.pid) {
			role_char = 'C';
		} else {
			role_char = (server.masterhost ? 'S' : 'M');
		}

		fprintf(fp, "%d:%c %s %c %s\n", (int)getpid(), role_char, buf, c[level], msg);
	}
	fflush(fp);

	if (!log_to_stdout) fclose(fp);
	if (server.syslog_enabled) syslog(syslogLevelMap[level], "%s", msg);
}

void redisLog(int level, const char *fmt, ...)
{
	va_list ap;
	char msg[REDIS_MAX_LOGMSG_LEN];

	if ((level & 0xff) < server.verbosity) return;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	redisLogRaw(level, msg);
}

void version(void)
{
	printf("Redis server v=%s sha=%s:%d malloc=%s bits=%d build=%llx\n",
		REDIS_VERSION,
		redisGitSHA1(),
		atoi(redisGitDirty()),
		ZMALLOC_LIB,
		sizeof(long) == 4 ? 32 : 64,
		(unsigned long long)redisBuildId());
	exit(0);	
}

void usage(void)
{
	fprintf(stderr, "Usage: ./redis-server [/path/to/redis.conf] [options]\n");
	fprintf(stderr, "		./redis-server - (read config from stdin)\n");
	fprintf(stderr, "		./redis-server -v or --version\n");
	fprintf(stderr, "		./redis-server -h or --help\n");
	fprintf(stderr, "		./redis-server --test-memory <megabytes>\n\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr,"       ./redis-server (run the server with default conf)\n");
	fprintf(stderr,"       ./redis-server /etc/redis/6379.conf\n");
	fprintf(stderr,"       ./redis-server --port 7777\n");
	fprintf(stderr,"       ./redis-server --port 7777 --slaveof 127.0.0.1 8888\n");
	fprintf(stderr,"       ./redis-server /etc/myredis.conf --loglevel verbose\n\n");
	fprintf(stderr,"Sentinel mode:\n");
	fprintf(stderr,"       ./redis-server /etc/sentinel.conf --sentinel\n");
	exit(1);
}

int main(int argc, char **argv)
{
	struct timeval tv;

#ifdef INI_SETPROCTITLE_REPLACEMENT
	spt_init(argc, argv);
#endif
	setlocale(LC_COLLATE, "");
	zmalloc_enable_thread_safeness();
	zmalloc_set_oom_handler(redisOutOfMemoryHandler);
	srand(time(NULL) ^ getpid());
	gettimeofday(&tv, NULL);
	dictSetHashFunctionSeed(tv.tv_sec ^ tv.tv_usec ^ getpid());
	server.sentinel_mode = checkForSentinelMode(argc, argv);
	initServerConfig();

	if (argc >= 2) {
		int j = 1;
		sds options = sdsempty();
		char *configFile = NULL;

		if (strcmp(argv[1], "-v") == 0 ||
			strcmp(argv[1], "--version") == 0) version();
		if (strcmp(argv[1], "-h") == 0 ||
			strcmp(argv[1], "--help") == 0) usage();

		if (argv[j][0] != '-' || argv[j][1] != '-') {
			configFile = argv[j++];	
		}
	
		while (j != argc) {
			if (argv[j][0] == '-' && argv[j][1] == '-') {
				if (!strcmp(argv[j], "--check-rdb")) {
					j++;
					continue;
				}	
				if (sdslen(options)) options = sdscat(options, "\n");
				options = sdscat(options, argv[j] + 2);
				options = sdscat(options, " ");
			} else {
				options = sdscatrepr(options, argv[j], strlen(argv[j]));
				options = sdscat(options, " ");
			}
			j++;
		}

		if (server.sentinel_mode && configFile && *configFile == '-') {
			redisLog(REDIS_WARNING,
				"Sentinel config from STDIN not allowed.");
			redisLog(REDIS_WARNING,
				"Sentinel needs config file on disk to save state. Exiting...");
			exit(1);
		}
		if (configFile) server.configFile = getAbsolutePath(configFile);
		resetServerSaveParams();
		loadServerConfig(configFile, options);
		sdsfree(options);
	} else {
		redisLog(REDIS_WARNING, "Warning: no config file specifiled, using the default config. In order to specify a config file use %s /path/to/%s.conf", argv[0], server.sentinel_mode ? "sentinel" : "redis");
	}
	
	return 0;
}
