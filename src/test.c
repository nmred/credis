#include <stdio.h>
#include "zmalloc.h"
#include "adlist.h"

#include <time.h>
#include <sys/time.h>

int main(void)
{
	struct timeval tv;
	char buf[64];
	gettimeofday(&tv, NULL);
	strftime(buf, sizeof(buf), "%d %b %H:%M:%S.", localtime(&tv.tv_sec));
	printf("%s\n", buf);

	size_t um;
	list *list;
	int arr[5] = {1, 2, 3, 4, 5};
	int i = 0;

	um = zmalloc_used_memory();
	printf("create list before :%ld\n", um);
	if ((list = listCreate()) == NULL) {
		printf("create list fail.");
	}
	
	int arrLen = sizeof(arr) / sizeof(arr[0]);
	for (i = 0; i < arrLen; i++) {
		listAddNodeHead(list, (arr + i));	
	}

	printf("list len:%ld\n", listLength(list));
	
	listIter *iter;
	listNode *node;
	iter = listGetIterator(list, AL_START_HEAD);
	while ((node = listNext(iter)) != NULL) {
		printf("node value: %d\n", *(int*)listNodeValue(node));
	}
	listReleaseIterator(iter);

	listRotate(list);

	iter = listGetIterator(list, AL_START_HEAD);
	while ((node = listNext(iter)) != NULL) {
		printf("node value: %d\n", *(int*)listNodeValue(node));
	}
	listReleaseIterator(iter);

	um = zmalloc_used_memory();
	printf("create list after: %ld\n", um);

	listRelease(list);
	um = zmalloc_used_memory();
	printf("free list after: %ld\n", um);
	return 0;
}
