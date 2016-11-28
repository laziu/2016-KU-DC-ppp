#pragma once
#ifndef __LCP_H__
#define __LCP_H__
#include "common.h"
#include <stdlib.h>
#include <stdbool.h>

// - accomp: Address and Control Field Compression flag
// - pcomp: Protocol Field Compression flag
// - Async Control Character Map
typedef struct {
	bool accomp;
	bool pcomp;
	u32 accm;
} lcp_conf;

// LCP configuration to use
extern lcp_conf lcp_srccf;
extern lcp_conf lcp_dstcf;

// check if LCP is opened (= data link layer available)
bool lcp_opened();
bool lcp_closed();

// do LCP step
void lcp_do();

#endif //__LCP_H__