// 2015410116
// gcc option: -std=gnu99
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <termios.h>

#include "physical.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "utils.h"

static tcflag_t select_baudrate(const char* str)
{
	if (strcmp(str, "B0"     ) == 0) return B0;
	if (strcmp(str, "B50"    ) == 0) return B50;
	if (strcmp(str, "B75"    ) == 0) return B75;
	if (strcmp(str, "B110"   ) == 0) return B110;
	if (strcmp(str, "B134"   ) == 0) return B134;
	if (strcmp(str, "B150"   ) == 0) return B150;
	if (strcmp(str, "B200"   ) == 0) return B200;
	if (strcmp(str, "B300"   ) == 0) return B300;
	if (strcmp(str, "B600"   ) == 0) return B600;
	if (strcmp(str, "B1200"  ) == 0) return B1200;
	if (strcmp(str, "B1800"  ) == 0) return B1800;
	if (strcmp(str, "B2400"  ) == 0) return B2400;
	if (strcmp(str, "B4800"  ) == 0) return B4800;
	if (strcmp(str, "B9600"  ) == 0) return B9600;
	if (strcmp(str, "B19200" ) == 0) return B19200;
	if (strcmp(str, "B38400" ) == 0) return B38400;
	if (strcmp(str, "B57600" ) == 0) return B57600;
	if (strcmp(str, "B115200") == 0) return B115200;
	if (strcmp(str, "B230400") == 0) return B230400;
	printf("unsupported baudrate: %s\n", str);
	return 0x12345678;
}

void exit_handler(int s)
{
	if (s == SIGINT) {
		if (!ppp_terminate) {
			printf("Connection finished by User Request.\n");
			ppp_terminate = true;
		}
	}
}

int main(int argc, char* argv[])
{
	// default configurations
	tcflag_t    f_baudrate      = B38400;
	const char*	f_serial_device = "/dev/ttyS0";
	bool        f_accomp        = true;
	bool        f_pcomp         = true;
	u32         f_asyncmap      = 0x00000000;
	byte        f_src_ip[4]     = { 10, 1, 3, 10 };
	byte        f_dst_ip[4]     = { 10, 1, 3, 20 };
	char* s_baud = "B38400";

	// recieve arguments
	if (argc > 1) {
		for (int i = 1; i < argc; ++i) {
			if (memcmp(argv[i], "B", 1) == 0) {	// baudrate
				tcflag_t sb = select_baudrate(argv[i]);
				if (sb != 0x12345678) {
					f_baudrate = sb;
					s_baud = argv[i];
				}
			}
			else if (memcmp(argv[i], "/", 1) == 0)	// serial device
				f_serial_device = argv[i];
			else if (strcmp(argv[i], "noaccomp") == 0)	// no accomp
				f_accomp = false;
			else if (strcmp(argv[i], "nopcomp") == 0)	// no pcomp
				f_pcomp = false;
			else if (strcmp(argv[i], "asyncmap") == 0) {	// asyncmap
				++i;
				u32 t_accm;
				if (i < argc && (sscanf(argv[i], "0x%x", &t_accm) == 1 || 
								 sscanf(argv[i], "0%o" , &t_accm) == 1 ||
								 sscanf(argv[i], "%u"  , &t_accm) == 1  ) )
					f_asyncmap = t_accm;
				else
					printf("asyncmap option format is not valid\n");
			}
			else {	// ip
				byte tb[8];
				if (sscanf(argv[i], "%hhu.%hhu.%hhu.%hhu:%hhu.%hhu.%hhu.%hhu", tb, tb+1, tb+2, tb+3, tb+4, tb+5, tb+6, tb+7) == 8) {
					memcpy(f_src_ip, tb, 4);
					memcpy(f_dst_ip, tb + 4, 4);
				}
				else	// none
					printf("unrecognized option: %s\n", argv[i]);
			}
		}
	}

	// print configurations
	printf("\t-- configurations ------\n");
	printf("\tbaudrate       %s\n", s_baud);
	printf("\tserial device  %s\n", f_serial_device);
	printf("\taccomp         %s\n", f_accomp ? "yes" : "no");
	printf("\tpcomp          %s\n", f_pcomp  ? "yes" : "no");
	printf("\tasyncmap       0x%08x\n", f_asyncmap);
	printf("\tsource ip      %d.%d.%d.%d\n", f_src_ip[0], f_src_ip[1], f_src_ip[2], f_src_ip[3]);
	printf("\tdest ip        %d.%d.%d.%d\n", f_dst_ip[0], f_dst_ip[1], f_dst_ip[2], f_dst_ip[3]);
	printf("\n");

	// initialize physical layer and others
	if (!phy_init(f_serial_device, f_baudrate))
		exit(-1);
	lcp_srccf = (lcp_conf){ .accomp = f_accomp, .pcomp = f_pcomp, .accm = f_asyncmap };
	for (int i = 0; i < 4; ++i)
		ipcp_srcip[i] = f_src_ip[i], ipcp_dstip[i] = f_dst_ip[i];

	fflush(stdin);
	fflush(stdout);
	rand32_init();

	if (signal(SIGINT, exit_handler) == SIG_ERR)
		printf("SIGINT Error: sigint=%d ID=%d\nCtrl+C interrupt unabled.\n", SIGINT, getpid());
	
	// run PPP state machine
	ppp_run();

	// close physical layer
	phy_close();

	printf("Waiting for cleanup ... \n");

    return 0;
}