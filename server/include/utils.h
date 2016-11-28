#pragma once
#ifndef __UTILS_H__
#define __UTILS_H__
#include "common.h"

#include <time.h>

void t_reset(struct timespec *ts);

long t_getms(struct timespec *ts);

void rand32_init();

u32 rand32();

#define BYTE_GET(n, offset) (((n) >> ((offset) << 3)) & 0xFF)
#define BYTE_2(a, b) ( (((a) << 8) & 0xFF00) | ((b) & 0x00FF) )
#define BYTE_4(a, b, c, d) (		\
	(((a) << 24) & 0xFF000000) |	\
	(((b) << 16) & 0x00FF0000) |	\
	(((c) <<  8) & 0x0000FF00) |	\
	( (d)        & 0x000000FF)   )
#define BYTE_4_BBUF(buf_name, i) (BYTE_4(buf_name.buf[i], buf_name.buf[i + 1], buf_name.buf[i + 2], buf_name.buf[i + 3]))

#ifdef DEBUG
#define LOG( ... ) printf( __VA_ARGS__ )
#else
#define LOG( ... )
#endif

#endif//__UTILS_H__