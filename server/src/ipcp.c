#include "ipcp.h"
#include "utils.h"
#include "ahdlc.h"
#include "physical.h"
#include "ppp.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// state of IPCP
typedef enum {
	ST_STOPPING = 5,
	ST_REQ_SENT,
	ST_ACK_RCVD,
	ST_ACK_SENT,
	ST_OPENED
} lcp_state;

// current IPCP state machine's state
static lcp_state state = ST_REQ_SENT;
// true if state is changed shortly before
static bool state_changed = true;

// IPCP buffer for sending frame
static byte_buf sbuf;

// IPCP configurations
static byte conf_id = 1;
static byte term_cnt = 0;

byte ipcp_srcip[4];
byte ipcp_dstip[4];

// used for timer
static struct timespec t_start;

bool ipcp_opened()
{
	return state == ST_OPENED;
}

// ---- behaviors ----------------------------------
static void gen_ipcpConfReqFrame()
{
	sbuf.buf[0] = 0x80;	// Protocol Code
	sbuf.buf[1] = 0x21;
	sbuf.buf[2] = 0x01;	// Conf-Req Code
	sbuf.buf[3] = conf_id;	// Identifier
	sbuf.buf[4] = 0x00;	// info size; set later
	sbuf.buf[5] = 0x10; 

	// IPCP
	sbuf.buf[6] = 0x02;	// Code
	sbuf.buf[7] = 0x06;
	sbuf.buf[8] = 0x00;	// VJ Compression
	sbuf.buf[9] = 0x2D;
	sbuf.buf[10] = 0x0F;	// Max Slot ID, fixed
	sbuf.buf[11] = 0x01;	// Comp Slot ID, fixed

	// IP
	sbuf.buf[12] = 0x03;	// Code
	sbuf.buf[13] = 0x06;
	sbuf.buf[14] = ipcp_srcip[0];
	sbuf.buf[15] = ipcp_srcip[1];
	sbuf.buf[16] = ipcp_srcip[2];
	sbuf.buf[17] = ipcp_srcip[3];
	sbuf.len = 18;
}

static void send_ipcpConfReq()
{
	// create IPCP Conf-Req frame data
	gen_ipcpConfReqFrame();

	// print sending data
	printf("send [IPCP Conf-Req id=0x%x", sbuf.buf[3]);
	printf(" <compress VJ %02x %02x>", sbuf.buf[10], sbuf.buf[11]);
	printf(" <addr %d.%d.%d.%d>]\n", ipcp_srcip[0], ipcp_srcip[1], ipcp_srcip[2], ipcp_srcip[3]);
	
	// send frame data
	hdlc_makeFrame(&sbuf);
	phy_send(&sbuf);
}

static bool check_rcvdIpcpConfAck()
{
	// check if rcvd_frame is IPCP Ack
	if (!rcvd_frame.len || rcvd_frame.buf[0] != 0x80 || rcvd_frame.buf[1] != 0x21)
		return false;

	if (rcvd_frame.buf[2] < 0x02 || 0x04 < rcvd_frame.buf[2])
		return false;

	if (rcvd_frame.buf[3] != conf_id)
		return false;

	if (rcvd_frame.buf[2] == 0x04) {	// if rcvd conf-rej
		printf("rcvd [IPCP Conf-Rej id=0x%x", rcvd_frame.buf[3]);
		for (int i = 6, iE = rcvd_frame.len; i < iE; ) {
			switch (rcvd_frame.buf[i++]) {
			case 2: // IPCP
				++i;
				printf(" <%s %02x %02x>",
					   rcvd_frame.buf[i] == 0x00 && rcvd_frame.buf[i+1] == 0x2D
					   ? "compress VJ" : "unknown ICMP",
					   rcvd_frame.buf[i+2], rcvd_frame.buf[i+3]);
				i += 4;
				break;
			case 3: // IP Address
				++i;
				printf(" <addr %d.%d.%d.%d>",
					   rcvd_frame.buf[i], rcvd_frame.buf[i+1],
					   rcvd_frame.buf[i+2], rcvd_frame.buf[i+3]);
				i += 4;
				break;
			}
		}
		printf("]\n");
		conf_id += 1;
	}
	else {	// if rcvd conf-ack or conf-nak
		// compare with conf-req
		gen_ipcpConfReqFrame();
		if (memcmp(sbuf.buf + 4, rcvd_frame.buf + 4, sbuf.len - 4) != 0)
			return false;	// if not matched ack

		printf("rcvd [IPCP Conf-%s", rcvd_frame.buf[2] == 0x02 ? "Ack" : "Nak");
		printf(" id=0x%x", rcvd_frame.buf[3]);
		printf(" <%s %02x %02x>",
			   rcvd_frame.buf[8] == 0x00 && rcvd_frame.buf[9] == 0x2D ? "compress VJ" : "unknown ICMP",
			   rcvd_frame.buf[10], rcvd_frame.buf[11]);
		printf(" <addr %d.%d.%d.%d>]\n", rcvd_frame.buf[14], 
			                             rcvd_frame.buf[15],
			                             rcvd_frame.buf[16],
			                             rcvd_frame.buf[17]);
	}

	rcvd_frame.len = 0;

	return rcvd_frame.buf[2] == 0x02;
}

static bool reply_rcvdIpcpConfReq()
{
	// check if rcvd_Frame is IPCP Configuration
	if (!rcvd_frame.len || rcvd_frame.buf[0] != 0x80 ||
		rcvd_frame.buf[1] != 0x21 || rcvd_frame.buf[2] != 0x01)
		return false;

	// printf rcvd Conf-Req info
	printf("rcvd [IPCP Conf-Req id=0x%x", rcvd_frame.buf[3]);
	
	byte accepted = 0;	// 0: Ack, 1: Reject VJ, 4: Nak, 8: None
	byte unknown_icmp[4];
	for (int i = 6, iE = rcvd_frame.len; i < iE; ) {
		switch (rcvd_frame.buf[i++]) {
		case 1: // IP Addresses
			accepted |= 4;
			++i;
			break;
		case 2: // IPCP
			if (rcvd_frame.buf[i++] != 0x06) {
				accepted |= 4;
				break;
			}
			if (rcvd_frame.buf[i  ] != 0x00 || rcvd_frame.buf[i+1] != 0x2D ||
				rcvd_frame.buf[i+2] != 0x0F || rcvd_frame.buf[i+3] != 0x01) {
				unknown_icmp[0] = rcvd_frame.buf[i];
				unknown_icmp[1] = rcvd_frame.buf[i+1];
				unknown_icmp[2] = rcvd_frame.buf[i+2];
				unknown_icmp[3] = rcvd_frame.buf[i+3];
				accepted |= 1;
				break;
			}
			printf(" <VJ Compress %02x %02x>", 0x0F, 0x01);
			i += 4;
			break;
		case 3:	// IP Addr
			if (rcvd_frame.buf[i++] != 0x06) {
				accepted |= 4;
				break;
			}
			if (rcvd_frame.buf[i  ] != ipcp_dstip[0] ||
				rcvd_frame.buf[i+1] != ipcp_dstip[1] ||
				rcvd_frame.buf[i+2] != ipcp_dstip[2] ||
				rcvd_frame.buf[i+3] != ipcp_dstip[3] ) {
				accepted |= 8;
				break;
			}
			printf(" <addr %d.%d.%d.%d>", rcvd_frame.buf[i  ],
				                          rcvd_frame.buf[i+1],
				                          rcvd_frame.buf[i+2],
				                          rcvd_frame.buf[i+3]);
			i += 4;
			break;
		}
	}
	printf("]\n");

	if (accepted == 8)	// not my concern
		return 0;

	if (accepted == 0 || (accepted & 4)) {	// Ack or Nak
		printf("send [IPCP Conf-%s id=0x%x", accepted? "Nak" : "Ack", rcvd_frame.buf[3]);
		printf(" <VJ compress %02x %02x>", 0x0F, 0x01);
		printf(" <addr %d.%d.%d.%d>]\n", ipcp_dstip[0], ipcp_dstip[1], ipcp_dstip[2], ipcp_dstip[3]);
		
		rcvd_frame.buf[2] = accepted? 0x03 : 0x02;
		hdlc_makeFrame(&rcvd_frame);
		phy_send(&rcvd_frame);
	}
	else {	// Reject
		printf("send [IPCP Conf-Rej id=0x%x", rcvd_frame.buf[3]);
		sbuf.buf[0] = 0x80;
		sbuf.buf[1] = 0x21;
		sbuf.buf[2] = 0x04;
		sbuf.buf[3] = rcvd_frame.buf[3];
		sbuf.len = 6;

		if (accepted & 1) {
			sbuf.buf[sbuf.len++] = 0x02;
			sbuf.buf[sbuf.len++] = 0x06;
			sbuf.buf[sbuf.len++] = unknown_icmp[0];
			sbuf.buf[sbuf.len++] = unknown_icmp[1];
			sbuf.buf[sbuf.len++] = unknown_icmp[2];
			sbuf.buf[sbuf.len++] = unknown_icmp[3];
			printf(" <unknown ICMP(%02x %02x) %02x %02x>",
				   unknown_icmp[0], unknown_icmp[1], unknown_icmp[2], unknown_icmp[3]);
		}
		printf("]\n");

		sbuf.buf[4] = BYTE_GET(sbuf.len - 2, 1);
		sbuf.buf[5] = BYTE_GET(sbuf.len - 2, 0);

		hdlc_makeFrame(&sbuf);
		phy_send(&sbuf);
	}

	rcvd_frame.len = 0;

	return accepted == 0;
}

// ---- state machine states -----------------------
static void ipcp_onStopping()
{
	
}

static void ipcp_onReqSent()
{
	// send IPCP Conf-Req in each timeout
	if (state_changed) {
		send_ipcpConfReq();
		t_reset(&t_start);
		state_changed = false;
	}

	// check timeout
	long diff = t_getms(&t_start);
	if (diff > 3000) {
		if (++term_cnt > 100) {
			printf("IPCP Configuration Request did not reached.\n");
			ppp_terminate = true;
		}
		state_changed = true;
	}
	else if (diff < 0) {
		LOG("\033[1;31mIPCP Conf-Req Sent: timer overflowed, reset.\n\033[0m");
		state_changed = true;
	}

	// check if recieved ACK -> ACK_RCVD
	if (check_rcvdIpcpConfAck()) {
		// successfully get Rcvd-ConfAck corresponds to Sent-ConfReq
		state = ST_ACK_RCVD;
		state_changed = true;
	}

	// check if recieved acceptable Conf-Req -> ACK_SENT
	if (reply_rcvdIpcpConfReq()) {
		state = ST_ACK_SENT;
		state_changed = true;
	}
}

static void ipcp_onAckRcvd()
{
	// wait until Conf-Req recieved
	if (state_changed) {
		t_reset(&t_start);
		state_changed = false;
	}

	// check timeout
	long diff = t_getms(&t_start);
	if (diff > 10000) {
		state = ST_REQ_SENT;
		state_changed = true;
	}
	else if (diff < 0) {
		LOG("\033[1;31mIPCP Conf-Req Wait: timer overflowed, reset.\n\033[0m");
		state_changed = true;
	}

	// check if recieved acceptable Conf-Req -> OPENED
	if (reply_rcvdIpcpConfReq()) {
		state = ST_OPENED;
		state_changed = true;
	}

}

static void ipcp_onAckSent()
{
	if (state_changed) {
		state_changed = false;
	}

	// check timeout and send LCP Conf-Req in each timeout
	long diff = t_getms(&t_start);
	if (diff > 3000) {
		if (++term_cnt > 100) {
			printf("IPCP Configuration Request did not reached.\n");
			ppp_terminate = true;
		}
		send_ipcpConfReq();
		t_reset(&t_start);
	}
	else if (diff < 0) {
		LOG("\033[1;31mLCP Conf-Ack Wait: timer overflowed, reset.\n\033[0m");
		t_reset(&t_start);
	}

	// check if recieved ACK -> OPENED
	if (check_rcvdIpcpConfAck()) {
		// successfully get Rcvd_ConfAck 
		state = ST_OPENED;
		state_changed = true;
	}

	// check if recieved acceptable Conf-Req
	reply_rcvdIpcpConfReq();
}

static void ipcp_onOpened()
{
	if (state_changed) {
		term_cnt = 0;
		state_changed = false;
	}

	// check if recieved Echo-Req
	if (reply_rcvdIpcpConfReq()) {
		term_cnt = 0;
		state = ST_ACK_SENT;
		state_changed = true;
	}
}

// ---- state machine selector ---------------------
void ipcp_do()
{

#ifdef DEBUG
	if (state_changed) {
		printf("\033[1;35mIPCP State: ");
		switch (state) {
		case ST_STOPPING: printf("STOPPING"); break;
		case ST_REQ_SENT: printf("REQ_SENT"); break;
		case ST_ACK_RCVD: printf("ACK_RCVD"); break;
		case ST_ACK_SENT: printf("ACK_SENT"); break;
		case ST_OPENED:   printf("OPENED"); break;
		}
		printf("\n\033[0m");
	}
#endif

	switch (state) {
	case ST_STOPPING:	ipcp_onStopping();	break;
	case ST_REQ_SENT:	ipcp_onReqSent();	break;
	case ST_ACK_RCVD:	ipcp_onAckRcvd();	break;
	case ST_ACK_SENT:	ipcp_onAckSent();	break;
	case ST_OPENED:		ipcp_onOpened();	break;
	}

	if (rcvd_frame.len && rcvd_frame.buf[0] == 0x80 && rcvd_frame.buf[1] == 0x21) {
		LOG("\033[1;31m\tRecieved IPCP Frame but it's not implemented.\n\033[0m");
		rcvd_frame.len = 0;
	}
}
