#pragma once
#ifndef __PPP_H__
#define __PPP_H__
#include "common.h"

extern volatile bool ppp_terminate;
extern byte_buf rcvd_frame;

// run PPP state machine.
void ppp_run();

#endif//__PPP_H__