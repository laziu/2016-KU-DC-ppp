#include "lcp.h"
#include "utils.h"
#include "ahdlc.h"
#include "physical.h"
#include "ppp.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// state of LCP
typedef enum {
//	ST_INIT = 0,
//	ST_STARTING = 1,
	ST_CLOSED   = 2,
//	ST_STOPPED  = 3,
	ST_CLOSING  = 4,
//	ST_STOPPING = 5,
	ST_REQ_SENT = 6,
	ST_ACK_RCVD = 7,
	ST_ACK_SENT = 8,
	ST_OPENED   = 9
} lcp_state;

// current LCP state machine's state
static lcp_state state = ST_REQ_SENT;
// true if state is changed shortly before
static bool state_changed = true;

// lcp buffer for sending frame
static byte_buf sbuf;

// lcp configurations
lcp_conf lcp_srccf = { .accomp = false, .pcomp = false, .accm = 0xFFFFFFFF };
lcp_conf lcp_dstcf = { .accomp = false, .pcomp = false, .accm = 0xFFFFFFFF };
static u32 magic_number;
static u32 dst_echo_mgnum;
static byte conf_id = 1;
static byte echo_id = 0;
static byte term_cnt = 0;

// used for timer
static struct timespec t_start;

bool lcp_opened()
{
	return state == ST_OPENED;
}

bool lcp_closed()
{
	return state == ST_CLOSED;
}

// ---- behaviors ----------------------------------
static void generate_lcpConfReqFrame()
{
	sbuf.buf[0] = 0xC0;	// Protocol Code 
	sbuf.buf[1] = 0x21;
	sbuf.buf[2] = 0x01;	// Conf-Req Code
	sbuf.buf[3] = conf_id;	// Identifier
	sbuf.buf[4] = 0x00;	// info size; set later
	sbuf.buf[5] = 0x00;
	sbuf.len = 6;

	// ACCM option
	if (lcp_srccf.accm != 0xFFFFFFFF) {
		sbuf.buf[sbuf.len++] = 0x02;
		sbuf.buf[sbuf.len++] = 0x06;
		sbuf.buf[sbuf.len++] = BYTE_GET(lcp_srccf.accm, 3);
		sbuf.buf[sbuf.len++] = BYTE_GET(lcp_srccf.accm, 2);
		sbuf.buf[sbuf.len++] = BYTE_GET(lcp_srccf.accm, 1);
		sbuf.buf[sbuf.len++] = BYTE_GET(lcp_srccf.accm, 0);
	}

	// magic number
	{
		sbuf.buf[sbuf.len++] = 0x05;
		sbuf.buf[sbuf.len++] = 0x06;
		sbuf.buf[sbuf.len++] = BYTE_GET(magic_number, 3);
		sbuf.buf[sbuf.len++] = BYTE_GET(magic_number, 2);
		sbuf.buf[sbuf.len++] = BYTE_GET(magic_number, 1);
		sbuf.buf[sbuf.len++] = BYTE_GET(magic_number, 0);
	}

	// pcomp option
	if (lcp_srccf.pcomp) {
		sbuf.buf[sbuf.len++] = 0x07;
		sbuf.buf[sbuf.len++] = 0x02;
	}

	// accomp option
	if (lcp_srccf.accomp) {
		sbuf.buf[sbuf.len++] = 0x08;
		sbuf.buf[sbuf.len++] = 0x02;
	}

	// set frame length
	sbuf.buf[4] = BYTE_GET(sbuf.len - 2, 1);
	sbuf.buf[5] = BYTE_GET(sbuf.len - 2, 0);
}

static void generate_lcpEchoFrame()
{
	sbuf.buf[0] = 0xC0; // Protocol Code
	sbuf.buf[1] = 0x21;
	sbuf.buf[2] = 0x09; // Echo-Req Code
	sbuf.buf[3] = echo_id; // Identifier
	sbuf.buf[4] = 0x00; // info size
	sbuf.buf[5] = 0x08;
	sbuf.buf[6] = BYTE_GET(magic_number, 3);
	sbuf.buf[7] = BYTE_GET(magic_number, 2);
	sbuf.buf[8] = BYTE_GET(magic_number, 1);
	sbuf.buf[9] = BYTE_GET(magic_number, 0);
	sbuf.len = 10;
}

static void send_lcpConfReq()
{
	// create LCP Conf-Req frame data 
	magic_number = rand32();
	generate_lcpConfReqFrame();

	// print sending data
	printf("send [LCP Conf-Req id=0x%x", sbuf.buf[3]);
	if (lcp_srccf.accm != 0xFFFFFFFF)
		printf(" <asyncmap 0x%x>", lcp_srccf.accm);
	printf(" <magic 0x%x>", magic_number);
	if (lcp_srccf.pcomp)
		printf(" <pcomp>");
	if (lcp_srccf.accomp)
		printf(" <accomp>");
	printf("]\n");
	// send frame data
	hdlc_makeFrame(&sbuf);
	phy_send(&sbuf);
}

static void send_lcpEchoReq()
{
	// create LCP Echo-Req frame data
	magic_number = rand32();
	generate_lcpEchoFrame();

	// print sending data
	printf("send [LCP Echo-Req id=0x%x magic=0x%x]\n", sbuf.buf[3], magic_number);

	// send frame data
	hdlc_makeFrame(&sbuf);
	phy_send(&sbuf);
}

static void send_lcpTermReq()
{
	sbuf.buf[0] = 0xC0;
	sbuf.buf[1] = 0x21;
	sbuf.buf[2] = 0x05;
	sbuf.buf[3] = conf_id;
	memcpy(sbuf.buf + 6, "pppimpl shutdown :0)>---<", 25);
	sbuf.len = 31;
	sbuf.buf[4] = 0x00;
	sbuf.buf[5] = 29u;

	printf("send [LCP Term-Req id=0x%x \"pppimpl shutdown :0)>---<\"]\n", conf_id);

	hdlc_makeFrame(&sbuf);
	phy_send(&sbuf);
}

static int check_rcvdLcpConfAck()
{
	// check if rcvd_frame is LCP Ack
	if (!rcvd_frame.len || rcvd_frame.buf[0] != 0xC0 || rcvd_frame.buf[1] != 0x21)
		return 0;

	if (rcvd_frame.buf[2] < 0x02 || 0x04 < rcvd_frame.buf[2])
		return 0;

	if (rcvd_frame.buf[3] != conf_id)
		return 0;

	int ret_value;
	switch (rcvd_frame.buf[2]) {
	case 0x02: ret_value =  1; break;	// Ack
	case 0x03: ret_value = -1; break;	// Nak
	case 0x04: ret_value = -2; break;	// Reject
	default:   ret_value =  0; break;	// no Ack
	}

	if (ret_value == -2) {	// if rcvd conf-reject
		printf("rcvd [LCP Conf-Rej id=0x%x", rcvd_frame.buf[3]);
		for (int i = 6, iE = rcvd_frame.len; i < iE; ++i) {
			switch (rcvd_frame.buf[i++]) {
			case 7:	// PFC
				printf(" <pcomp>");
				lcp_srccf.pcomp = false;
				break;
			case 8:	// ACFC
				printf(" <accomp>");
				lcp_srccf.accomp = false; 
				break;
			}
		}
		printf("]\n");
		conf_id += 1;
	}
	else {	// if rcvd conf-ack or conf-nak
		// compare with conf-req
		generate_lcpConfReqFrame();
		bool target = sbuf.len == rcvd_frame.len &&
			memcmp(sbuf.buf + 4, rcvd_frame.buf + 4, sbuf.len - 4) == 0;
		if (!target) // if not matched ack
			return 0;

		printf("rcvd [LCP Conf-%s", ret_value == 1 ? "Ack" : ret_value == -1 ? "Nak" : "???"); 
		printf(" id=0x%x", rcvd_frame.buf[3]);
		if (lcp_srccf.accm != 0xFFFFFFFF)
			printf(" <asyncmap 0x%x>", lcp_srccf.accm);
		printf(" <magic 0x%x>", magic_number);
		if (lcp_srccf.pcomp)
			printf(" <pcomp>");
		if (lcp_srccf.accomp)
			printf(" <accomp>");
		printf("]\n");
	}

	// clear rcvd_frame
	rcvd_frame.len = 0;

	return ret_value;
}

static bool check_rcvdLcpEchoAck()
{
	// check if rcvd_frame is LCP Ack
	if (!rcvd_frame.len)
		return false;

	generate_lcpEchoFrame();
	sbuf.buf[2] = 0x0A;
	sbuf.buf[6] = BYTE_GET(dst_echo_mgnum, 3);
	sbuf.buf[7] = BYTE_GET(dst_echo_mgnum, 2);
	sbuf.buf[8] = BYTE_GET(dst_echo_mgnum, 1);
	sbuf.buf[9] = BYTE_GET(dst_echo_mgnum, 0);
	if (sbuf.len != rcvd_frame.len || memcmp(sbuf.buf, rcvd_frame.buf, sbuf.len) != 0)
		return false;

	printf("rcvd [LCP Echo-Rep id=0x%x magic=0x%x]\n", rcvd_frame.buf[3], BYTE_4_BBUF(rcvd_frame, 6));

	// clear rcvd_frame
	rcvd_frame.len = 0;

	++echo_id;

	return true;
}

static bool check_rcvdLcpTermAck()
{
	if (!rcvd_frame.len || rcvd_frame.buf[0] != 0xC0 || rcvd_frame.buf[1] != 0x21)
		return false;

	if (rcvd_frame.buf[2] != 0x06 || rcvd_frame.buf[3] != conf_id)
		return false;

	printf("rcvd [LCP Term-Ack id=0x%x]\n", rcvd_frame.buf[3]);

	rcvd_frame.len = 0;

	return true;
}

static bool reply_rcvdLcpConfReq() 
{
	// check if rcvd_frame is LCP Configuration
	if (!rcvd_frame.len || rcvd_frame.buf[0] != 0xC0 || 
		rcvd_frame.buf[1] != 0x21 || rcvd_frame.buf[2] != 0x01)
		return false;

	// print rcvd Conf-Req info
	printf("rcvd [LCP Conf-Req id=0x%x", rcvd_frame.buf[3]);

	// default implicit settings
	lcp_dstcf = (lcp_conf){ .accomp = false, .pcomp = false, .accm = 0xFFFFFFFF };

	byte accepted = 0; // 0: Ack, 1: Reject pcomp, 2: Reject accomp, 4: Reject MRU, 8: Nak
	u32 mgnum = 0;	// rcvd magic number
	int sz = 0;		// rcvd mru
	for (int i = 6, iE = rcvd_frame.len; i < iE; ) {
		switch (rcvd_frame.buf[i++]) {
#define NAK_IF(exp)    if (exp) { accepted  = 8; break; }
#define REJ_IF(exp, n) if (exp) { accepted |= n; break; }
		case 1:	// Maximum Recieve Unit
			NAK_IF(rcvd_frame.buf[i++] != 0x06)
			sz = BYTE_4_BBUF(rcvd_frame, i);
			printf(" <MRU %d>", sz);
			i += 4;
			REJ_IF(sz > BUFFER_SIZE / 2 - 20, 4)
			break;
		case 2:	// ACCM
			NAK_IF(rcvd_frame.buf[i++] != 0x06)
			lcp_dstcf.accm = BYTE_4_BBUF(rcvd_frame, i);
			if (lcp_dstcf.accm != 0xFFFFFFFF)
				printf(" <asyncmap 0x%x>", lcp_dstcf.accm);
			i += 4;
			break;
		case 5: // Magic Number
			NAK_IF(rcvd_frame.buf[i++] != 0x06)
			mgnum = BYTE_4_BBUF(rcvd_frame, i);
			printf(" <magic 0x%x>", mgnum);
			i += 4;
			break;
		case 7: // PFC
			NAK_IF(rcvd_frame.buf[i++] != 0x02)
			printf(" <pcomp>");
			REJ_IF(!lcp_srccf.pcomp, 1)
			lcp_dstcf.pcomp = true;
			break;
		case 8: // ACFC
			NAK_IF(rcvd_frame.buf[i++] != 0x02)
			printf(" <accomp>");
			REJ_IF(!lcp_srccf.accomp, 2)
			lcp_dstcf.accomp = true;
			break;
		default:
			NAK_IF(true)
#undef NAK_IF
#undef REJ_IF
		}
	}		
	printf("]\n");

	if (accepted == 0 || accepted == 8) {	// Ack or Nak
		// print sent Conf-Reply info
		printf("send [LCP Conf-%s id=0x%x", accepted == 0? "Ack" : "Nak", rcvd_frame.buf[3]);
		if (lcp_dstcf.accm != 0xFFFFFFFF)
			printf(" <asyncmap 0x%x>", lcp_dstcf.accm);
		printf(" <magic 0x%x>", magic_number);
		if (lcp_dstcf.pcomp)
			printf(" <pcomp>");
		if (lcp_dstcf.accomp)
			printf(" <accomp>");
		printf("]\n");

		rcvd_frame.buf[2] = accepted == 0? 0x02 : 0x03;
		hdlc_makeFrame(&rcvd_frame);
		phy_send(&rcvd_frame);
	}
	else {	// Reject
		printf("send [LCP Conf-Rej id=0x%x", rcvd_frame.buf[3]);
		sbuf.buf[0] = 0xC0;	// protocol field
		sbuf.buf[1] = 0x21; 
		sbuf.buf[2] = 0x04;	// Conf-Rej
		sbuf.buf[3] = rcvd_frame.buf[3];	// id
		sbuf.len = 6;

		if (accepted & 1) {	// pcomp rejected
			sbuf.buf[sbuf.len++] = 0x07;
			sbuf.buf[sbuf.len++] = 0x02;
			printf(" <pcomp>");
		}
		if (accepted & 2) {	// accomp rejected
			sbuf.buf[sbuf.len++] = 0x08;
			sbuf.buf[sbuf.len++] = 0x02;
			printf(" <accomp>");
		}
		if (accepted & 4) { // mru rejected
			sbuf.buf[sbuf.len++] = 0x01;
			sbuf.buf[sbuf.len++] = 0x06;
			sbuf.buf[sbuf.len++] = BYTE_GET(sz, 3);
			sbuf.buf[sbuf.len++] = BYTE_GET(sz, 2);
			sbuf.buf[sbuf.len++] = BYTE_GET(sz, 1);
			sbuf.buf[sbuf.len++] = BYTE_GET(sz, 0);
			printf(" <MRU %d>", sz);
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

static bool reply_rcvdLcpEchoReq()
{
	// check if rcvd_frame is LCP Echo-Req
	if (!rcvd_frame.len || rcvd_frame.buf[0] != 0xC0 ||
		rcvd_frame.buf[1] != 0x21 || rcvd_frame.buf[2] != 0x09 ||
		rcvd_frame.buf[4] != 0x00 || rcvd_frame.buf[5] != 0x08)
		return false;

	dst_echo_mgnum = BYTE_4_BBUF(rcvd_frame, 6);
	// print rcvd Echo-Req and sent Echo-Reply info
	printf("rcvd [LCP Echo-Req id=0x%x magic=0x%x]\n", rcvd_frame.buf[3], dst_echo_mgnum);
	printf("send [LCP Echo-Rep id=0x%x magic=0x%x]\n", rcvd_frame.buf[3], magic_number);

	// send ack
	generate_lcpEchoFrame();
	sbuf.buf[2] = 0x0A;
	sbuf.buf[3] = rcvd_frame.buf[3];
	hdlc_makeFrame(&sbuf);
	phy_send(&sbuf);

	rcvd_frame.len = 0;

	return true;
}

static bool reply_rcvdLcpTermReq()
{
	if (!rcvd_frame.len || rcvd_frame.buf[0] != 0xC0 || rcvd_frame.buf[1] != 0x21)
		return false;

	if (rcvd_frame.buf[2] != 0x05)
		return false;

	printf("rcvd [LCP Term-Req id=0x%x \"", rcvd_frame.buf[3]);
	for (int i = 6; i < rcvd_frame.len; ++i)
		printf("%c", rcvd_frame.buf[i]);
	printf("\"]\n");

	printf("send [LCP Term-Ack id=0x%x]\n", rcvd_frame.buf[3]);

	sbuf.buf[0] = 0xC0;
	sbuf.buf[1] = 0x21;
	sbuf.buf[2] = 0x06;
	sbuf.buf[3] = rcvd_frame.buf[3];
	sbuf.buf[4] = 0x00;
	sbuf.buf[5] = 0x04;
	sbuf.len = 6;

	hdlc_makeFrame(&sbuf);
	phy_send(&sbuf);

	rcvd_frame.len = 0;

	return true;
}

// ---- state machine states -----------------------
static void lcp_onClosing()
{
	if (state_changed) {
		++conf_id;
		send_lcpTermReq();
		t_reset(&t_start);
		state_changed = false;
	}

	// check timeout
	long diff = t_getms(&t_start);
	if (diff > 3000) {
		++term_cnt;
		if (term_cnt >= 3) {
			printf("LCP Termination Request did not reached.\n");
			ppp_terminate = true;
			state = ST_CLOSED;
		}
		state_changed = true;
	}
	else if (diff < 0) {
		LOG("\033[1;31mLCP Term-Req Sent: timer overflowed, reset.\n\033[0m");
		state_changed = true;
	}

	if (check_rcvdLcpTermAck() || reply_rcvdLcpTermReq()) {
		ppp_terminate = true;
		state = ST_CLOSED;
		state_changed = true;
	}
}

static void lcp_onReqSent() 
{
	// send LCP Conf-Req in each timeout
	if (state_changed) {
		send_lcpConfReq();
		t_reset(&t_start);
		state_changed = false;
	}

	// check timeout
	long diff = t_getms(&t_start);
	if (diff > 3000) {
		if (++term_cnt > 10) {
			printf("LCP Configuration Request did not reached.\n");
			ppp_terminate = true;
		}
		state_changed = true;
	}
	else if (diff < 0) {
		LOG("\033[1;31mLCP Conf-Req Sent: timer overflowed, reset.\n\033[0m");
		state_changed = true;
	}

	// check if recieved ACK -> ACK_RCVD
	int ack_state = check_rcvdLcpConfAck();
	if (ack_state > 0) {
		// successfully get Rcvd-ConfAck corresponds to Sent-ConfReq
		state = ST_ACK_RCVD;
		state_changed = true;
	}
	else if (ack_state < 0) {
		// get Rcvd_ConfNak -> reset 
		state_changed = true;
	}

	// check if recieved acceptable Conf-Req -> ACK_SENT
	if (reply_rcvdLcpConfReq()) {
		state = ST_ACK_SENT;
		state_changed = true;
	}

	// check if Term-Req recieved
	if (reply_rcvdLcpTermReq()) {
		ppp_terminate = true;
		state = ST_CLOSED;
		state_changed = true;
	}
}

static void lcp_onAckRcvd() 
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
		LOG("\033[1;31mLCP Conf-Req Wait: timer overflowed, reset.\n\033[0m");
		state_changed = true;
	}

	// check if recieved acceptable Conf-Req -> OPENED
	if (reply_rcvdLcpConfReq()) {
		state = ST_OPENED;
		state_changed = true;
	}

	// check if Term-Req recieved
	if (reply_rcvdLcpTermReq()) {
		ppp_terminate = true;
		state = ST_CLOSED;
		state_changed = true;
	}
}

static void lcp_onAckSent() 
{
	if (state_changed) {
		state_changed = false;
	}

	// check timeout and send LCP Conf-Req in each timeout
	long diff = t_getms(&t_start);
	if (diff > 3000) {
		if (++term_cnt > 10) {
			printf("LCP Configuration Request did not reached.\n");
			ppp_terminate = true;
		}
		send_lcpConfReq();
		t_reset(&t_start);
	}
	else if (diff < 0) {
		LOG("\033[1;31mLCP Conf-Ack Wait: timer overflowed, reset.\n\033[0m");
		t_reset(&t_start);
	}

	// check if recieved ACK -> OPENED
	int ack_state = check_rcvdLcpConfAck();
	if (ack_state > 0) {
		// successfully get Rcvd_ConfAck 
		state = ST_OPENED;
		state_changed = true;
	}
	else if (ack_state < 0) {
		send_lcpConfReq();
		t_reset(&t_start);
	}

	// check if recieved acceptable Conf-Req
	reply_rcvdLcpConfReq();

	// check if Term-Req recieved
	if (reply_rcvdLcpTermReq()) {
		ppp_terminate = true;
		state = ST_CLOSED;
		state_changed = true;
	}
}

static void lcp_onOpened()
{
	static bool acked = true;
	// send Echo-Req in each timeout
	if (state_changed) {
		if (!acked) {
			printf("\033[1;31mLCP Echo timeout.\n\033[0m");
			if (++term_cnt > 3) {
				printf("Connection losted.\n");
				ppp_terminate = true;
			}
		}
		send_lcpEchoReq();
		t_reset(&t_start);
		acked = false;
		state_changed = false;
	}

	// check timeout
	long diff = t_getms(&t_start);
	if (diff > 30000) {
		state_changed = true;
	}
	else if (diff < 0) {
		LOG("\033[1;31mLCP Echo: timer overflowed, reset.\n\033[0m");
		t_reset(&t_start);
	}

	// check if recieved ACK
	if (check_rcvdLcpEchoAck()) {
		term_cnt = 0;
		acked = true;
	}

	// check if recieved Conf-Req
	if (reply_rcvdLcpConfReq()) {
		term_cnt = 0;
		state = ST_ACK_SENT;
		state_changed = true;
	}

	// check if recieved Echo-Req
	reply_rcvdLcpEchoReq();

	// check if recieved Term-Req
	if (reply_rcvdLcpTermReq()) {
		ppp_terminate = true;
		printf("LCP Terminated by peer.\n");
		state = ST_CLOSED;
		state_changed = true;
	}
}

// ---- state machine selector ---------------------
void lcp_do()
{
#ifdef DEBUG
	if (state_changed) {
		printf("\033[1;35mLCP State: ");
		switch (state) {
		case ST_CLOSED:   printf("CLOSED"); break;
		case ST_CLOSING:  printf("STOPPING"); break;
		case ST_REQ_SENT: printf("REQ_SENT"); break;
		case ST_ACK_RCVD: printf("ACK_RCVD"); break;
		case ST_ACK_SENT: printf("ACK_SENT"); break;
		case ST_OPENED:   printf("OPENED"); break;
		}
		printf("\n\033[0m");
	}
#endif

	switch (state) {
	case ST_CLOSING:	lcp_onClosing();	break;
	case ST_REQ_SENT:	lcp_onReqSent();	break;
	case ST_ACK_RCVD:	lcp_onAckRcvd();	break;
	case ST_ACK_SENT:	lcp_onAckSent();	break;
	case ST_OPENED:		lcp_onOpened();		break;
	}

	if (ppp_terminate && state != ST_CLOSED && state != ST_CLOSING) {
		state = ST_CLOSING;
		term_cnt = 0;
		state_changed = true;
	}

	if (rcvd_frame.len && rcvd_frame.buf[0] == 0xC0 && rcvd_frame.buf[1] == 0x21) {
		LOG("\033[1;31m\tRecieved LCP Frame but it's not implemented.\n\033[0m");
		rcvd_frame.len = 0;
	}
}
