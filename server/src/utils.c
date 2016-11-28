#include "utils.h"
#include <time.h>
#include <stdlib.h>

void t_reset(struct timespec *ts)
{
	clock_gettime(CLOCK_MONOTONIC, ts);
}

long t_getms(struct timespec *ts)
{
	struct timespec cur;
	clock_gettime(CLOCK_MONOTONIC, &cur);
	return (cur.tv_sec  - ts->tv_sec ) * 1E3
		 + (cur.tv_nsec - ts->tv_nsec) / 1E6;
}

static u32 seed[4];

void rand32_init()
{
	srand(time(0));
	seed[0] = (unsigned)rand();
	seed[1] = (unsigned)rand();
	seed[2] = (unsigned)rand();
	seed[3] = (unsigned)rand();
}

u32 rand32()
{
	// use xorshift128 pseudo-random algorithm
	u32 t = seed[0];
	t ^= t << 11;
	t ^= t >> 8;
	seed[0] = seed[1];
	seed[1] = seed[2];
	seed[2] = seed[3];
	seed[3] ^= seed[3] >> 19;
	seed[3] ^= t;
	return seed[3];
}