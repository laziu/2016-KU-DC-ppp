#include "ahdlc.h"

#include "lcp.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

// ----- implementing FCS -----------------------------------------
// fcs reference: https://tools.ietf.org/html/rfc1662#appendix-C.2
static u16 fcstable[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static inline u16 fcs16(byte *fbuf, size_t len)
{
	u16 fcs = 0xffff;	// initial FCS value
	while (len--)
		fcs = (fcs >> 8) ^ fcstable[(fcs ^ *fbuf++) & 0xff];
	return fcs;
}

inline void makefcs(byte_buf *fbuf)
{
	u16 fcs = fcs16(fbuf->buf, fbuf->len) ^ 0xffff;	// complement of fcs
	fbuf->buf[fbuf->len    ] =  fcs       & 0x00ff;	// LSB
	fbuf->buf[fbuf->len + 1] = (fcs >> 8) & 0x00ff;	// MSB
	fbuf->len += 2;
}

inline bool checkfcs(byte_buf *fbuf)
{
	return fcs16(fbuf->buf, fbuf->len) == 0xf0b8;	// good final FCS value
}
// ----------------------------------------------------------------

// ----- implementing ACCM ----------------------------------------
static byte_buf tbuf;

void async(byte_buf *fbuf, u32 accm)
{
	// asynchronize data
	tbuf.len = 0;
	for (int i = 0; i < fbuf->len; ++i) {
		if (tbuf.len >= BUFFER_SIZE) { // buffer size check
			printf("\033[1;31mbuffer overflow while asynchronize\n\033[0m");
			return;
		}

		byte b = fbuf->buf[i];
		if (b == 0x7D || b == 0x7E ||
			(b <= 0x1F && ((accm >> b) & 0x01))) 
		{
			tbuf.buf[tbuf.len++] = 0x7D;
			if (tbuf.len >= BUFFER_SIZE) { // buffer size check
				printf("\033[1;31mbuffer overflow while asynchronize\n\033[0m");
				return;
			}
			tbuf.buf[tbuf.len++] = 0x20 ^ b;
		}
		else
			tbuf.buf[tbuf.len++] = b;
	}

	// store new data in buf
	memcpy(fbuf->buf, tbuf.buf, tbuf.len);
	fbuf->len = tbuf.len;
}

void unasync(byte_buf *fbuf)
{
	// un-asynchronize data
	tbuf.len = 0;
	for (int i = 0; i < fbuf->len; ++i)
		tbuf.buf[tbuf.len++] = 
			fbuf->buf[i] == 0x7D ? fbuf->buf[++i] ^ 0x20 : fbuf->buf[i];
	
	// store new data in buf
	memcpy(fbuf->buf, tbuf.buf, tbuf.len);
	fbuf->len = tbuf.len;
}
// ----------------------------------------------------------------

bool hdlc_getInfo(byte_buf *fbuf)
{
	// un-asynchronize data
	tbuf.len = 0;
	for (int i = 0; i < fbuf->len; ++i)
		tbuf.buf[tbuf.len++] = 
			fbuf->buf[i] == 0x7D ? fbuf->buf[++i] ^ 0x20 : fbuf->buf[i];

	// halt if FCS invalid
	if (!checkfcs(&tbuf)) {
		LOG("\033[1;31m\tRecieved frame has CRC error.\n\033[0m");
		return false;
	}

	// delete hdlc frame form
	if (tbuf.buf[0] == 0xFF && tbuf.buf[1] == 0x03)
		memmove(tbuf.buf, tbuf.buf + 2, tbuf.len -= 2);
	
	// check LCP pcomp 
	if (lcp_dstcf.pcomp && (tbuf.buf[0] == 0xFD || tbuf.buf[0] == 0x21)) {
		fbuf->buf[0] = 0x00;
		memcpy(fbuf->buf + 1, tbuf.buf, tbuf.len - 2);
		fbuf->len = tbuf.len - 1;
	}
	else {
		memcpy(fbuf->buf, tbuf.buf, tbuf.len - 2);
		fbuf->len = tbuf.len - 2;
	}

#ifdef DEBUG
	printf("\033[1;36mIN\t");
	for (int i = 0; i <= fbuf->len; ++i) {
		if (i == fbuf->len) {
			for (; i % 16 != 0; ++i)
				printf("   ");
			printf("\t\033[0m");
			for (int j = i - 16; j < fbuf->len; ++j)
				if (fbuf->buf[j] < 0x20)
					printf("\033[33m.\033[0m");
				else if (fbuf->buf[j] > 0x7F)
					printf("\033[35m.\033[0m");
				else
					printf("%c", fbuf->buf[j]);
			break;
		}
		if (i && i % 16 == 0) {
			printf("\t\033[0m");
			for (int j = i - 16; j < i; ++j)
				if (fbuf->buf[j] < 0x20)
					printf("\033[33m.\033[0m");
				else if (fbuf->buf[j] > 0x7F)
					printf("\033[35m.\033[0m");
				else
					printf("%c", fbuf->buf[j]);
			printf("\033[1;36m\n\t");
		}
		printf("%02x ", fbuf->buf[i]);
	}
	printf("\n");
#endif
	return true;
}

bool hdlc_makeFrame(byte_buf *fbuf)
{
#ifdef DEBUG
	printf("\033[1;36mOUT\t");
	for (int i = 0; i <= fbuf->len; ++i) {
		if (i == fbuf->len) {
			for (; i % 16 != 0; ++i)
				printf("   ");
			printf("\t\033[0m");
			for (int j = i - 16; j < fbuf->len; ++j)
				if (fbuf->buf[j] < 0x20)
					printf("\033[33m.\033[0m");
				else if (fbuf->buf[j] > 0x7F)
					printf("\033[35m.\033[0m");
				else
					printf("%c", fbuf->buf[j]);
			break;
		}
		if (i && i % 16 == 0) {
			printf("\t\033[0m");
			for (int j = i - 16; j < i; ++j)
				if (fbuf->buf[j] < 0x20)
					printf("\033[33m.\033[0m");
				else if (fbuf->buf[j] > 0x7F)
					printf("\033[35m.\033[0m");
				else
					printf("%c", fbuf->buf[j]);
			printf("\033[1;36m\n\t");
		}
		printf("%02x ", fbuf->buf[i]);
	}
	printf("\n");
#endif

	u32 use_accm = lcp_opened() ? lcp_dstcf.accm : 0xffffffff;
	bool use_accomp = lcp_opened() && lcp_dstcf.accomp;
	bool use_pcomp  = lcp_opened() && lcp_dstcf.pcomp;

	// append CRC value first
	tbuf.len = 0;
	if (!use_accomp) {
		tbuf.buf[tbuf.len++] = 0xFF;
		tbuf.buf[tbuf.len++] = 0x03;
	}
	int pcomped = use_pcomp && fbuf->buf[0] == 0x00;
	memcpy(tbuf.buf + tbuf.len, fbuf->buf + pcomped, fbuf->len - pcomped);
	tbuf.len += fbuf->len - pcomped;
	makefcs(&tbuf);

	fbuf->buf[fbuf->len    ] = tbuf.buf[tbuf.len - 2];
	fbuf->buf[fbuf->len + 1] = tbuf.buf[tbuf.len - 1];

	// asynchronize data
	tbuf.len = 0;
	if (!use_accomp) {
		tbuf.buf[tbuf.len++] = 0xFF;
		if ((use_accm >> 0x03) & 1) {
			tbuf.buf[tbuf.len++] = 0x7D;
			tbuf.buf[tbuf.len++] = 0x23;
		}
		else
			tbuf.buf[tbuf.len++] = 0x03;
	}

	for (int i = pcomped; i < fbuf->len + 2; ++i) {
		if (tbuf.len >= BUFFER_SIZE) { // buffer size check
			printf("\033[1;31mbuffer overflow while asynchronize\n\033[0m");
			return false;
		}

		byte b = fbuf->buf[i];
		if (b == 0x7D || b == 0x7E ||
			(b <= 0x1F && ((use_accm >> b) & 0x01))) {
			tbuf.buf[tbuf.len++] = 0x7D;
			if (tbuf.len >= BUFFER_SIZE) { // buffer size check
				printf("\033[1;31mbuffer overflow while asynchronize\n\033[0m");
				return false;
			}
			tbuf.buf[tbuf.len++] = 0x20 ^ b;
		}
		else
			tbuf.buf[tbuf.len++] = b;
	}

	// store new data in buf
	memcpy(fbuf->buf, tbuf.buf, tbuf.len);
	fbuf->len = tbuf.len;

	return true;
}
