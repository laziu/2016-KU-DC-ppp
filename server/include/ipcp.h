#pragma once
#ifndef __IPCP_H__
#define __IPCP_H__
#include "common.h"
#include <stdlib.h>
#include <stdbool.h>

extern byte ipcp_srcip[4];
extern byte ipcp_dstip[4];

bool ipcp_opened();

void ipcp_do();

#endif//__IPCP_H__