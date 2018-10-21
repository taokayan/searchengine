
#include "stdafx.h"
#include "KKVector.h"

void testKKVector()
{
	printf("KKVector tests... pass if no assert fails...\n");
	KKVector<int> v;
	
	if (!v.runtimeAssert()) printf("init failed\n");
	if (v.length() != 0 || !v.runtimeAssert()) printf("length failed!\n");
	v.push_back(10);
	if (v.length() != 1 || !v.runtimeAssert()) printf("length failed! 2\n");
	if (v[0] != 10) printf("v[0] failed\n");
	v.push_back(20);
	if (v.length() != 2 || !v.runtimeAssert()) printf("length failed! 3\n");
	v.push_back(30);
	v.push_back(40);
	v.push_back(50);
	if (v.length() != 5 || !v.runtimeAssert()) printf("length failed! 4\n");
	if (v[0] != 10) printf("v[0] failed 2\n");
	if (v[1] != 20 || v[2] != 30 || v[3] != 40 || v[4] != 50) printf("v[i] failed\n");
	v.push_front(-10);
	if (v.length() != 6 || !v.runtimeAssert()) printf("length failed 5\n");
	if (v[0] != -10) printf("v[0] failed 3\n");
	if (v[0] != -10 || v[1] != 10 || v[2] != 20 || v[5] !=50) printf("v[i] failed 2\n");
	if (v.pop_back() != 50) printf("pop back failed\n");
	if (v.length() != 5 || !v.runtimeAssert()) printf("len failed 6\n");
	if (v.pop_front() != -10 || !v.runtimeAssert()) printf("pop front failed\n");
	if (v.pop_front() != 10 || !v.runtimeAssert()) printf("pop front failed 2\n");
	if (v.pop_front() != 20 || !v.runtimeAssert()) printf("pop front failed 3\n");
	if (v.length() != 2 || !v.runtimeAssert()) printf("len failed 7\n");
    if (v.pop_back() != 40) printf("pop back failed 4\n");
	if (v.pop_back() != 30) printf("pop back failed 3\n");
	if (v.length() != 0 || !v.runtimeAssert()) printf("finaly failed\n");
}
