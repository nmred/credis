#ifndef __TESTHELP_H
#define __TESTHELP_H

int __failed_test = 0;
int __test_num = 0;

#define test_cond(descr, _c) do { \
	__test_num++; printf("%d - %s: ", __test_num, descr); \
	if (_c) printf("PASSED\n"); else { printf("FAILED\n"); __failed_test++;} \
} while(0);

#define test_report() do { \
	printf("%d tests, %d passed, %d failed\n", __test_num, \
			__test_num - __failed_test, __failed_test); \
	if (__failed_test) { \
		printf("=== WARNING === We have failed tests here...\n"); \
		exit(1); \
	} \
} while(0);

#endif
