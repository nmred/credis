#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "sha1.h"

int main(void)
{
	int i;
	unsigned char hash[20];
	unsigned char buf[] = "aaaa";
	SHA1_CTX ctx;
	SHA1Init(&ctx);
	SHA1Update(&ctx, buf, strlen((char *)buf));
	SHA1Final(hash, &ctx);
	for(i=0;i<20;i++)
		printf("%02x", hash[i]);
	printf("\n");
	return 0;
}
