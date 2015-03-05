#include "redis.h"

static struct {
	const char *name;
	const int value;
} validSyslogFacilities[] = {
	{"user", LOG_USER},
	{"local0", LOG_LOCAL0},
	{"local1", LOG_LOCAL1},
	{"local2", LOG_LOCAL2},
	{"local3", LOG_LOCAL3},
	{"local4", LOG_LOCAL4},
	{"local5", LOG_LOCAL5},
	{"local6", LOG_LOCAL6},
	{"local7", LOG_LOCAL7},
	{NULL, 0}
};

/*------------------------------------------------------------------
 * 解析配置文件
 *-----------------------------------------------------------------*/

//int supervisedToMode(const char *str);

int yesnotoi(char *s)
{
	if (!strcasecmp(s, "yes")) return 1;
	else if (!strcasecmp(s, "no")) return 0;
	else return -1;
}

void appendServerSaveParams(time_t seconds, int changes)
{
	server.saveparams = zrealloc(server.saveparams, sizeof(struct saveparam) * (server.saveparamslen + 1));
	server.saveparams[server.saveparamslen].seconds = seconds;
	server.saveparams[server.saveparamslen].changes = changes;
	server.saveparamslen++;
}

void resetServerSaveParams(void)
{
	zfree(server.saveparams);
	server.saveparams = NULL;
	server.saveparamslen = 0;
}

void loadServerConfigFromString(char *config)
{
	char *err = NULL;	
	int linenum = 0, totlines, i;
	int slaveof_linenum = 0;
	sds *lines;

	lines = sdssplitlen(config, strlen(config), "\n", 1, &totlines);

	for (i = 0; i < totlines; i++) {
		sds *argv;
		int argc;

		linenum = i + 1;
		lines[i] = sdstrim(lines[i], " \t\n\r");

		// 跳过注释和空行
		if (lines[i][0] == '#' || lines[i][0] == '\0') continue;

		// 切分配置项
		argv = sdssplitargs(lines[i], &argc);	
		if (argv == NULL) {
			err = "Unbalance quotes in configuration line";
			goto loaderr;
		}

		if (argc == 0) {
			sdsfreesplitres(argv, argc);
			continue;
		}
		sdstolower(argv[0]);

		if (!strcasecmp(argv[0], "timeout") && argc == 2) {
			server.maxidletime = atoi(argv[1]);
			if (server.maxidletime < 0) {
				err = "Invalid timeout value"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "tcp-keepalive") && argc == 2) {
			server.tcpkeepalive = atoi(argv[1]);
			if (server.tcpkeepalive < 0) {
				err = "Invalid tcp-keepalive value"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "port") && argc == 2) {
			server.port = atoi(argv[1]);
			if (server.port < 0 || server.port > 65535) {
				err = "Invalid port"; goto loaderr;
			}	
		} else if (!strcasecmp(argv[0], "tcp-backlog") && argc == 2) {
			server.tcp_backlog = atoi(argv[1]);
			if (server.tcp_backlog < 0 || server.tcp_backlog > 65535) {
				err = "Invalid backlog value"; goto loaderr;
			}	
		} else if (!strcasecmp(argv[0], "bind") && argc >= 2) {
			int j, addresses = argc - 1;
			if (addresses > REDIS_BINDADDR_MAX) {
				err = "Too many bind addresses specified"; goto loaderr;
			}

			for (j = 0; j < addresses; j++) {
				server.bindaddr[j] = zstrdup(argv[j + 1]);
			}	
			server.bindaddr_count = addresses;
		} else if (!strcasecmp(argv[0], "unixsocket") && argc == 2) {
			server.unixsocket = zstrdup(argv[1]);		
		} else if (!strcasecmp(argv[0], "unixsocketperm") && argc == 2) {
			errno = 0;
			server.unixsocketperm = (mode_t)strtol(argv[1], NULL, 8);
			if (errno || server.unixsocketperm > 0777) {
				err = "Invalid socket file permissions"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "save")) {
			if (argc == 3) {
				int seconds = atoi(argv[1]);
				int changes = atoi(argv[2]);
				if (seconds < 1 || changes < 0) {
					err = "Invalid save parameters"; goto loaderr;
				}
				appendServerSaveParams(seconds, changes);
			} else if (argc == 2 && !strcasecmp(argv[1], "")){
				resetServerSaveParams();	
			}
		} else if (!strcasecmp(argv[0], "dir") && argc == 2) {
			if (chdir(argv[1]) == -1) {
				redisLog(REDIS_WARNING, "Can't chdir to '%s': %s",
					argv[1], strerror(errno));
				exit(1);		
			}
		} else if (!strcasecmp(argv[0], "loglevel") && argc == 2) {
			if (!strcasecmp(argv[1], "debug")) server.verbosity = REDIS_DEBUG;
			else if (!strcasecmp(argv[1], "verbose")) server.verbosity = REDIS_VERBOSE;
			else if (!strcasecmp(argv[1], "notice")) server.verbosity = REDIS_NOTICE;
			else if (!strcasecmp(argv[1], "warning")) server.verbosity = REDIS_WARNING;
			else {
				err = "Invalid log level. Must be one of debug, notice, warning";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "logfile") && argc == 2) {
			FILE *logfp;

			zfree(server.logfile);
			server.logfile = zstrdup(argv[1]);
			if (server.logfile[0] != '\0') {
				logfp = fopen(server.logfile, "a");
				if (logfp == NULL) {
					err = sdscatprintf(sdsempty(),
						"Cant't open the log file: %s", strerror(errno));
					goto loaderr;
				}
				fclose(logfp);
			}
		} else if (!strcasecmp(argv[0], "syslog-enabled") && argc == 2) {
			if ((server.syslog_enabled = yesnotoi(argv[1])) == -1) {
				err = "argument must be 'yes' or 'no'"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "syslog-ident") && argc == 2) {
			if (server.syslog_ident) zfree(server.syslog_ident);
			server.syslog_ident = zstrdup(argv[1]);
		} else if (!strcasecmp(argv[0], "syslog-facility") && argc == 2) {
			int i;
			for (i = 0; validSyslogFacilities[i].name; i++) {
				if (!strcasecmp(validSyslogFacilities[i].name, argv[1])) {
					server.syslog_facility = validSyslogFacilities[i].value;
					break;
				}
			}

			if (!validSyslogFacilities[i].name) {
				err = "Invalid log facility. Must be one of USER or between LOCAL0-LOCAL7";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "databases") && argc == 2) {
			server.dbnum = atoi(argv[1]);
			if (server.dbnum < 1) {
				err = "Invalid number of database"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "include") && argc == 2) {
			loadServerConfig(argv[1], NULL);
		} else if (!strcasecmp(argv[0], "maxclients") && argc == 2) {
			server.maxclients = atoi(argv[1]);
			if (server.maxclients < 1) {
				err = "Invalid max clients limit"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "maxmemory") && argc == 2) {
			server.maxmemory = memtoll(argv[1], NULL);
		} else if (!strcasecmp(argv[0], "maxmemory-policy") && argc == 2) {
			if (!strcasecmp(argv[1], "volatile-lru")) {
				server.maxmemory_policy = REDIS_MAXMEMORY_VOLATILE_LRU;
			} else if (!strcasecmp(argv[1], "volatile-random")) {
				server.maxmemory_policy = REDIS_MAXMEMORY_VOLATILE_RANDOM;
			} else if (!strcasecmp(argv[1], "volatile-ttl")) {
				server.maxmemory_policy = REDIS_MAXMEMORY_VOLATILE_TTL;
			} else if (!strcasecmp(argv[1], "allkeys-lru")) {
				server.maxmemory_policy = REDIS_MAXMEMORY_VOLATILE_LRU;
			} else if (!strcasecmp(argv[1], "allkeys-random")) {
				server.maxmemory_policy = REDIS_MAXMEMORY_VOLATILE_RANDOM;
			} else if (!strcasecmp(argv[1], "noeviction")) {
				server.maxmemory_policy = REDIS_MAXMEMORY_NO_EVICTION;
			} else {
				err = "Invalid maxmemory policy";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "maxmemory-samples") && argc == 2) {
			server.maxmemory_samples = atoi(argv[1]);
			if (server.maxmemory_samples <= 0) {
				err = "maxmemory-samples must be 1 or greater";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "slaveof") && argc == 3) {
			slaveof_linenum = linenum;
			server.masterhost = sdsnew(argv[1]);
			server.masterport = atoi(argv[2]);
			server.repl_state = REDIS_REPL_CONNECT;
		} else if (!strcasecmp(argv[0], "repl-ping-slave-period") && argc == 2) {
			server.repl_ping_slave_period = atoi(argv[1]);
			if (server.repl_ping_slave_period <= 0) {
				err = "repl-ping-slave-period must be 1 or greater";
				goto loaderr;	
			}
		} else if (!strcasecmp(argv[0], "repl-timeout") && argc == 2) {
			server.repl_timeout = atoi(argv[1]);
			if (server.repl_timeout <= 0) {
				err = "repl-timeout must be 1 or greater";
				goto loaderr;	
			}
		} else if (!strcasecmp(argv[0], "repl-disable-tcp-nodelay") && argc == 2) {
			if ((server.repl_disable_tcp_nodelay = yesnotoi(argv[1])) == -1) {
				err = "argument must be 'yes' or 'no'"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "repl-diskless-sync") && argc == 2) {
			if ((server.repl_diskless_sync = yesnotoi(argv[1])) == -1) {
				err = "argument must be 'yes' or 'no'"; goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "repl-diskless-sync-delay") && argc == 2) {
			server.repl_diskless_sync_delay = atoi(argv[1]);
			if (server.repl_diskless_sync_delay < 0) {
				err = "repl-diskless-sync-delay can't be negative";
				goto loaderr;
			}
		} else if (!strcasecmp(argv[0], "repl-backlog-size") && argc == 2) {
			long long size = memtoll(argv[1], NULL);
			if (size <= 0) {
				err = "repl-backlog-size must be 1 or greater.";
				goto loaderr;
			}
			//resizeReplicationBacklog(size);
		} else if (!strcasecmp(argv[0], "masterauth") && argc == 2) {
			server.masterauth = zstrdup(argv[1]);
		} else if (!strcasecmp(argv[0], "slave-serve-stale-data") && argc == 2) {
			if ((server.repl_serve_stale_data = yesnotoi(argv[1])) == -1) {
				err = "argument must be 'yes' or 'no'"; goto loaderr;
			}
		} else {
			err = "Bad directive or wrong number of arguments"; goto loaderr;
		}
		sdsfreesplitres(argv, argc);
	}

	sdsfreesplitres(lines, totlines);
	return;

loaderr:
	fprintf(stderr, "\n*** FATAL CONFIG FILE ERROR ***\n");
	fprintf(stderr, "Reading the configuration file, at line %d\n", linenum);
	fprintf(stderr, ">>> '%s'\n", lines[i]);
	fprintf(stderr, "%s\n", err);
	exit(1);
}

void loadServerConfig(char *filename, char *options)
{
	sds config = sdsempty();
	char buf[REDIS_CONFIGLINE_MAX + 1];

	if (filename) {
		FILE *fp;

		if (filename[0] == '-' && filename[1] == '\0') {
			fp = stdin;
		} else {
			if ((fp = fopen(filename, "r")) == NULL) {
				redisLog(REDIS_WARNING,
					"Fatal error, can't open config file '%s'", filename);
				exit(1);	
			}
		}
		while(fgets(buf, REDIS_CONFIGLINE_MAX + 1, fp) != NULL) {
			config = sdscat(config, buf);
		}

		if (fp != stdin) fclose(fp);
	}

	if (options) {
		config = sdscat(config, "\n");
		config = sdscat(config, options);
	}

	loadServerConfigFromString(config);
	sdsfree(config);
}
