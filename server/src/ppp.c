#include "ppp.h"
#include <stdio.h>
#include <stdbool.h>

#include "physical.h"
#include "ahdlc.h"
#include "lcp.h"
#include "ipcp.h"
#include "utils.h"

typedef enum {
	PH_DEAD,
	PH_ESTABLISH,
//	PH_AUTH,
	PH_NETWORK,
	PH_TERMINATE
} ppp_phase;

// current PPP state machine's state
static ppp_phase state = PH_DEAD;

// true if the program is finished
volatile bool ppp_terminate = false;
static bool program_finished = false;

byte_buf rcvd_frame;

static u16 get_input()
{
	// read frame data
	if (rcvd_frame.len == 0) {
		phy_read();
		phy_getFrame(&rcvd_frame);
		// check if frame exists
		if (rcvd_frame.len)
			if (!hdlc_getInfo(&rcvd_frame))
				rcvd_frame.len = 0;

		if (rcvd_frame.len) {
			LOG("\033[1;36m%02x %02x\t", rcvd_frame.buf[0], rcvd_frame.buf[1]);
			u16 prtc = BYTE_2(rcvd_frame.buf[0], rcvd_frame.buf[1]);
			switch (prtc) {
			case 0xC021: LOG("PPP LCP");  break;
			case 0x8021: LOG("PPP IPCP"); break;
			case 0x0021: LOG("ICMP, not implemented. dropped");		rcvd_frame.len = 0;	break;
			case 0x80FD: LOG("PPP CCP, not implemented. dropped");	rcvd_frame.len = 0;	break;
			case 0x00FD: LOG("PPP Compressed Datagram, not implemented. dropped");	rcvd_frame.len = 0;	break;
			default:     LOG("Unknown Protocol, dropped");			rcvd_frame.len = 0; break;
			}
			LOG("\n\033[0m");
			return prtc;
		}
	}
	return 0;
}

static void ppp_onDead()
{
	// try to send LCP conf-request 
	while(!ppp_terminate && !lcp_opened()) {
		// read frame data
		get_input();
		// activate LCP state machine
		lcp_do();
	}

	// go to PH_ESTABLISH when conf negotiation succeed
	state = PH_ESTABLISH;
}

static void ppp_onEstablish()
{
	// try to send IPCP Conf-request
	while (!ppp_terminate && lcp_opened() && !ipcp_opened()) {
		// read frame data
		get_input();
		// activate LCP and IPCP state machine
		lcp_do();
		ipcp_do();
	}

	// go to PH_NETWORK when conf negotiation succeed
	state = PH_NETWORK;
}

static void ppp_onNetwork()
{
	printf("local  IP address %d.%d.%d.%d\n", ipcp_srcip[0], ipcp_srcip[1], ipcp_srcip[2], ipcp_srcip[3]);
	printf("remote IP address %d.%d.%d.%d\n", ipcp_dstip[0], ipcp_dstip[1], ipcp_dstip[2], ipcp_dstip[3]);
	while (!ppp_terminate && lcp_opened() && ipcp_opened()) {
		get_input();
		lcp_do();
		ipcp_do();
	}
	printf("network closed.\n");

	state = PH_TERMINATE;
}

static void ppp_onTerminate()
{
	while (!lcp_closed()) {
		if (get_input() != 0xC021)
			rcvd_frame.len = 0;
		lcp_do();
	}
	printf("Connection terminated.\n");
	program_finished = true;
}

void ppp_run()
{
	while (!program_finished) {
		switch (state) {
		case PH_DEAD:		ppp_onDead();		break;
		case PH_ESTABLISH:	ppp_onEstablish();	break;
		case PH_NETWORK:	ppp_onNetwork();	break;
		case PH_TERMINATE:	ppp_onTerminate();	break;
		}
	}
}
