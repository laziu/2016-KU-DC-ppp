#pragma once
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>
#include <stdbool.h>

typedef unsigned char	byte;
typedef unsigned short	u16;
typedef unsigned int	u32;

#define BUFFER_SIZE 16384

typedef struct {
	size_t len;
	byte buf[BUFFER_SIZE];
} byte_buf;

#endif //__COMMON_H__