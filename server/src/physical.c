#include "physical.h"

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include <termios.h>

static int fd;
static struct termios tio, origtio;
static byte_buf ibuf, obuf;
static int ifhead, iftail;

bool phy_init(const char *device_name, tcflag_t baud)
{
	// open linux serial device file.
	// O_RDWR: open for reading / writing
	// O_NOCTTY: terminal device should not control the process
	// O_NONBLOCK: no thread safe while reading
	fd = open(device_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {	// failed to open device
		perror(device_name);
		return false;
	}

	// configure serial port settings
	memset(&tio, 0, sizeof(tio));
	// set input modes
	// IGNPAR: ignore framing errors and parity errors
	// ICRNL: translate carriage return to newline on input
	tio.c_iflag = IGNPAR | ICRNL;
	// set control modes
	// baud: baudrate, predefined (ex. B38400)
	// CRTSCTS: enable RTS/CTS flow control
	// CS8: 8bit no parity, 1 stopbit
	// CLOCAL: local connection, no modem control
	// CREAD: enable character receive
	tio.c_cflag = baud | CRTSCTS | CS8 | CLOCAL | CREAD;
	// output mode is raw

	// save original config
	tcgetattr(fd, &origtio);

	// set new config
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &tio);

	ibuf.len = 0;
	iftail = 0;

	return true;
}

bool phy_close()
{
	// restore the previous config
	tcsetattr(fd, TCSANOW, &origtio);
	fd = 0;
	return true;
}

size_t phy_read()
{
	// streaming data and concat to ibuf
	int rdlen = read(fd, ibuf.buf + ibuf.len, BUFFER_SIZE - ibuf.len);
	
#ifdef DEBUG
	if (rdlen) {
		printf("\033[33mrcvd\t");
		for (int i = 0; i < rdlen; ++i) {
			if (i && i % 16 == 0)
				printf("\n\t");
			printf("%02x ", ibuf.buf[ibuf.len + i]);
		}
		printf("\n\033[0m");
	}
#endif

	ibuf.len += rdlen;
	return rdlen;
}

void phy_getFrame(byte_buf *fbuf)
{
	fbuf->len = 0;

	// iftail: frame rear
	if (iftail <= 0)
		iftail = 1;

	for (; iftail < ibuf.len; ++iftail)
		// split before frame delimeter
		if (ibuf.buf[iftail] == 0x7E && ibuf.buf[iftail - 1] != 0x7E) {
			// ifhead: frame front
			for (ifhead = 0; ibuf.buf[ifhead] == 0x7E; ++ifhead);

			// move first frame to fbuf and align remained data
			fbuf->len = iftail - ifhead;
			memcpy(fbuf->buf, ibuf.buf + ifhead, fbuf->len);
			ibuf.len -= iftail + 1;
			memmove(ibuf.buf, ibuf.buf + iftail + 1, ibuf.len);
			iftail = 0;

#ifdef DEBUG
			printf("\033[1;33mGet:\t");
			for (int i = 0; i < fbuf->len; ++i) {
				if (i && i % 16 == 0) 
					printf("\n\t");
				printf("%02x ", fbuf->buf[i]);
			}
			printf("\n\033[0m");
#endif
			return;
		}
}

size_t phy_send(const byte_buf *fbuf)
{
	memcpy(obuf.buf + 1, fbuf->buf, fbuf->len);
	obuf.buf[0] = obuf.buf[fbuf->len + 1] = 0x7E;
	obuf.len = fbuf->len + 2;

#ifdef DEBUG
	printf("\033[32msent\t");
	for (int i = 0; i < obuf.len; ++i) {
		if (i && i % 16 == 0)
			printf("\n\t");
		printf("%02x ", obuf.buf[i]);
	}
	printf("\n\033[0m");
#endif

	return write(fd, obuf.buf, obuf.len);
}
