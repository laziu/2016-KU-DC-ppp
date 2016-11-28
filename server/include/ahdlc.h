#pragma once
#ifndef __AHDLC_H__
#define __AHDLC_H__
#include "common.h"

#include <stdlib.h>
#include <stdbool.h>

// append fcs after data.
// - fbuf: byte_buf storing data
void makefcs(byte_buf *fbuf);

// check fcs of frame data.
// - fbuf: byte_buf storing data includes fcs in rear
// return true if fcs is correct
bool checkfcs(byte_buf *fbuf);

// asynchronize data with using accm
// - fbuf: byte_buf storing raw data
// - accm: Async Control Character Map
void async(byte_buf *fbuf, u32 accm);

// un-asynchronize data by 0x7D XOR method.
// - fbuf: byte_buf storing asynchronized data
void unasync(byte_buf *fbuf);

// get HDLC Information from raw frame data
// - fbuf: byte_buf storing raw frame data
// return HDLC frame validation. buf/len won't be changed if false.
bool hdlc_getInfo(byte_buf *fbuf);

// make AHDLC Frame from HDLC Information data
// - fbuf: byte_buf storing info data
// return true if process succeed. false if buf size is not enough.
bool hdlc_makeFrame(byte_buf *fbuf);

#endif //__AHDLC_H__