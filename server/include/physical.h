#pragma once
#ifndef __PHYSICAL_H__
#define __PHYSICAL_H__
#include "common.h"

#include <stdlib.h>
#include <stdbool.h>
#include <termios.h>

// init physical layer.
// - device_name:  serial file device (ex. /dev/ttyS0)
// - baud: baud rate 
// return terminated flag
bool phy_init(const char *device_name, tcflag_t baud);

// close physical layer.
// return terminated flag.
bool phy_close();

// read byte stream.
// return length of received data.
size_t phy_read();

// get entire frame recieved splited by delimeter.
// - fbuf: byte_buf to get data
// return length of data. negative when buffer size is not enough.
void phy_getFrame(byte_buf *fbuf);

// send byte stream.
// - fbuf: byte_buf storing data
// return length of sended data.
size_t phy_send(const byte_buf *fbuf);

#endif //__PHYSICAL_H__