#include <string.h>

#include "release.h"
#include "version.h"
#include "crc64.h"

char *redisGitSHA1(void)
{
	return REDIS_GIT_SHA1;
}

char *redisGitDirty(void)
{
	return REDIS_GIT_DIRTY;
}

uint64_t redisBuildId(void)
{
	char *buildid = REDIS_VERSION REDIS_BUILD_ID REDIS_GIT_DIRTY REDIS_GIT_SHA1;
	return crc64(0, (unsigned char *)buildid, strlen(buildid));	
}
