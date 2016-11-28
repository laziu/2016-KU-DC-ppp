#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>   
#include <termios.h>   
#include <fcntl.h>   
#include <signal.h>   
#include <unistd.h>   
#include <sys/un.h>   
#include <sys/ioctl.h>   
#include <linux/ip.h>   
#include <sys/ioctl.h>   
#include <errno.h>   
#include <sys/types.h>   
#include <sys/socket.h>   
#include <linux/netlink.h>   
#include <sys/stat.h>   
#include <time.h>   
#include <assert.h>   
#include <fcntl.h>   
    
#define BAUDRATE B38400   
#define SERIALDEVICE "/dev/ttyS0"   
   
#define PPPINITFCS16 0xffff /* Initial FCS value */   
#define PPPGOODFCS16 0xf0b8 /* Good final FCS value */   
typedef unsigned short u16;   
typedef unsigned long u32;   
   
unsigned char buf[10000];   
unsigned char buf1[5000];   
unsigned char buf2[10000];   
unsigned char buf3[10000];   
unsigned char tmp_buf[10000];   
unsigned char tmp2_buf[10000];   
unsigned char tmp3_buf[10000];   
int fd;   
   
//------------------------------------------------------------------------------------------------------------------------------------------------   
static u16 fcstab[256] = {   
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
u16 pppfcs16(u16 fcs, char* cp, int len){   
   
    while (len--)   
    fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];   
    return (fcs);   
}   
/*     
* How to use the fcs   
*/   
void makefcs(unsigned char* cp, int len) {   
    u16 trialfcs;   
    /* add on output */   
    trialfcs = pppfcs16( PPPINITFCS16, cp, len );   
    trialfcs ^= 0xffff; /* complement */   
    cp[len] = (trialfcs & 0x00ff); /* least significant byte first */   
    cp[len+1] = ((trialfcs >> 8) & 0x00ff);   
}   
int checkfcs(unsigned char* cp, int len) {   
    u16 trialfcs;   
    /* check on input */   
    trialfcs = pppfcs16( PPPINITFCS16, cp, len + 2 );   
    if ( trialfcs == PPPGOODFCS16 )   
    { return 1;}   
    else { return 0;}   
}   
//------------------------------------------------------------------------------------------------------------------------------------------------   
   
//PPP outline   
typedef enum phase {   
    phDead =0, phEstablish, phAuthenticate, phNetwork, phTerminate   
}ppp_phase;   
   
//Events   
enum ppp_event {   
    evUp = 0, evDown, evOpen, evClose, evTOp, evTOm,   
    evRCRp, evRCRm, evRCA, evRCN, evRTR, evRTA,   
    evRUC, evRXJp, evRXJm, evRXR   
};   
   
//States   
typedef enum state {   
    stInitial = 0, stStarting, stClosed, stStopped, stClosing, stStopping,   
    stReqSent, stAckRcvd, stAckSent, stOpened,   
    stNoChange   
}xcp_state;   
   
//Actions   
enum xcp_action {   
    acNull =0 , acIrc, acScr, acTls, acTlf, acStr, acSta, acSca, acScn, acScj,   
    acTld, acTlu, acZrc, acSer   
};   
      
//PPP State Machine table   
const struct ppp_dispatch {   
    enum state next;   
    enum xcp_action act[3];   
} ppp_dispatch[10][16] = {    //{new state,action}   
    { /* stInitial */   
    /* evUp */    { stClosed, { acNull, acNull, acNull } },   
    /* evDown */  { stNoChange, { acNull, acNull, acNull } },   
    /* evOpen */  { stStarting, { acTls, acNull, acNull } },   
    /* evClose */ { stClosed, { acNull, acNull, acNull } },   
    /* evTOp */   { stNoChange, { acNull, acNull, acNull } },   
    /* evTOm */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRCRp */  { stNoChange, { acNull, acNull, acNull } },   
    /* evRCRm */  { stNoChange, { acNull, acNull, acNull } },   
    /* evRCA */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRCN */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRTR */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRTA */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRUC */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRXJp */  { stNoChange, { acNull, acNull, acNull } },   
    /* evRXJm */  { stNoChange, { acNull, acNull, acNull } },   
    /* evRXR */   { stNoChange, { acNull, acNull, acNull } },   
    },   
    { /* stStarting */   
    /* evUp */    { stReqSent, { acIrc, acScr, acNull } },   
    /* evDown */  { stNoChange, { acNull, acNull, acNull } },   
    /* evOpen */  { stStarting, { acNull, acNull, acNull } },   
    /* evClose */ { stInitial, { acTlf, acNull, acNull } },   
    /* evTOp */   { stNoChange, { acNull, acNull, acNull } },   
    /* evTOm */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRCRp */  { stNoChange, { acNull, acNull, acNull } },   
    /* evRCRm */  { stNoChange, { acNull, acNull, acNull } },   
    /* evRCA */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRCN */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRTR */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRTA */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRUC */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRXJp */  { stNoChange, { acNull, acNull, acNull } },   
    /* evRXJm */  { stNoChange, { acNull, acNull, acNull } },   
    /* evRXR */   { stNoChange, { acNull, acNull, acNull } },   
    },   
    { /* stClosed */   
    /* evUp */    { stNoChange, { acNull, acNull, acNull } },   
    /* evDown */  { stInitial, { acNull, acNull, acNull } },   
    /* evOpen */  { stReqSent, { acIrc, acScr, acNull } },   
    /* evClose */ { stClosed, { acNull, acNull, acNull } },   
    /* evTOp */   { stNoChange, { acNull, acNull, acNull } },   
    /* evTOm */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRCRp */  { stClosed, { acSta, acNull, acNull } },   
    /* evRCRm */  { stClosed, { acSta, acNull, acNull } },   
    /* evRCA */   { stClosed, { acSta, acNull, acNull } },   
    /* evRCN */   { stClosed, { acSta, acNull, acNull } },   
    /* evRTR */   { stClosed, { acSta, acNull, acNull } },   
    /* evRTA */   { stClosed, { acNull, acNull, acNull } },   
    /* evRUC */   { stClosed, { acScj, acNull, acNull } },   
    /* evRXJp */  { stClosed, { acNull, acNull, acNull } },   
    /* evRXJm */  { stClosed, { acTlf, acNull, acNull } },   
    /* evRXR */   { stClosed, { acNull, acNull, acNull } },   
    },   
    { /* stStopped */   
    /* evUp */    { stNoChange, { acNull, acNull, acNull } },   
    /* evDown */  { stStarting, { acTls, acNull, acNull } },   
    /* evOpen */  { stStopped, { acNull, acNull, acNull } },   
    /* evClose */ { stClosed, { acNull, acNull, acNull } },   
    /* evTOp */   { stNoChange, { acNull, acNull, acNull } },   
    /* evTOm */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRCRp */  { stAckSent, { acIrc, acScr, acSca } },   
    /* evRCRm */  { stReqSent, { acIrc, acScr, acScn } },   
    /* evRCA */   { stStopped, { acSta, acNull, acNull } },   
    /* evRCN */   { stStopped, { acSta, acNull, acNull } },   
    /* evRTR */   { stStopped, { acSta, acNull, acNull } },   
    /* evRTA */   { stStopped, { acNull, acNull, acNull } },   
    /* evRUC */   { stStopped, { acScj, acNull, acNull } },   
    /* evRXJp */  { stStopped, { acNull, acNull, acNull } },   
    /* evRXJm */  { stStopped, { acTlf, acNull, acNull } },   
    /* evRXR */   { stStopped, { acNull, acNull, acNull } },   
    },   
    { /* stClosing */   
    /* evUp */    { stNoChange, { acNull, acNull, acNull } },   
    /* evDown */  { stInitial, { acNull, acNull, acNull } },   
    /* evOpen */  { stStopping, { acNull, acNull, acNull } },   
    /* evClose */ { stClosing, { acNull, acNull, acNull } },   
    /* evTOp */   { stClosing, { acStr, acNull, acNull } },   
    /* evTOm */   { stClosed, { acTlf, acNull, acNull } },   
    /* evRCRp */  { stClosing, { acNull, acNull, acNull } },   
    /* evRCRm */  { stClosing, { acNull, acNull, acNull } },   
    /* evRCA */   { stClosing, { acNull, acNull, acNull } },   
    /* evRCN */   { stClosing, { acNull, acNull, acNull } },   
    /* evRTR */   { stClosing, { acSta, acNull, acNull } },   
    /* evRTA */   { stClosed, { acTlf, acNull, acNull } },   
    /* evRUC */   { stClosing, { acScj, acNull, acNull } },   
    /* evRXJp */  { stClosing, { acNull, acNull, acNull } },   
    /* evRXJm */  { stClosed, { acTlf, acNull, acNull } },   
    /* evRXR */   { stClosing, { acNull, acNull, acNull } },   
    },   
    { /* stStopping */   
    /* evUp */    { stNoChange, { acNull, acNull, acNull } },   
    /* evDown */  { stInitial, { acNull, acNull, acNull } },   
    /* evOpen */  { stStopping, { acNull, acNull, acNull } },   
    /* evClose */ { stClosing, { acNull, acNull, acNull } },   
    /* evTOp */   { stStopping, { acStr, acNull, acNull } },   
    /* evTOm */   { stStopped, { acTlf, acNull, acNull } },   
    /* evRCRp */  { stStopping, { acNull, acNull, acNull } },   
    /* evRCRm */  { stStopping, { acNull, acNull, acNull } },   
    /* evRCA */   { stStopping, { acNull, acNull, acNull } },   
    /* evRCN */   { stStopping, { acNull, acNull, acNull } },   
    /* evRTR */   { stStopping, { acSta, acNull, acNull } },   
    /* evRTA */   { stStopped, { acTlf, acNull, acNull } },   
    /* evRUC */   { stStopping, { acScj, acNull, acNull } },   
    /* evRXJp */  { stStopping, { acNull, acNull, acNull } },   
    /* evRXJm */  { stStopped, { acTlf, acNull, acNull } },   
    /* evRXR */   { stStopping, { acNull, acNull, acNull } },   
    },   
    { /* stReqSent */   
    /* evUp */    { stNoChange, { acNull, acNull, acNull } },   
    /* evDown */  { stStarting, { acNull, acNull, acNull } },   
    /* evOpen */  { stReqSent, { acNull, acNull, acNull } },   
    /* evClose */ { stClosing, { acIrc, acStr, acNull } },   
    /* evTOp */   { stReqSent, { acScr, acNull, acNull } },   
    /* evTOm */   { stStopped, { acTlf, acNull, acNull } },   
    /* evRCRp */  { stAckSent, { acSca, acNull, acNull } },   
    /* evRCRm */  { stReqSent, { acScn, acNull, acNull } },   
    /* evRCA */   { stAckRcvd, { acIrc, acNull, acNull } },   
    /* evRCN */   { stReqSent, { acIrc, acScr, acNull } },   
    /* evRTR */   { stReqSent, { acSta, acNull, acNull } },   
    /* evRTA */   { stReqSent, { acNull, acNull, acNull } },   
    /* evRUC */   { stReqSent, { acScj, acNull, acNull } },   
    /* evRXJp */  { stReqSent, { acNull, acNull, acNull } },   
    /* evRXJm */  { stStopped, { acTlf, acNull, acNull } },   
    /* evRXR */   { stReqSent, { acNull, acNull, acNull } },   
    },   
    { /* stAckRcvd */   
    /* evUp */    { stNoChange, { acNull, acNull, acNull } },   
    /* evDown */  { stStarting, { acNull, acNull, acNull } },   
    /* evOpen */  { stAckRcvd, { acNull, acNull, acNull } },   
    /* evClose */ { stClosing, { acIrc, acStr, acNull } },   
    /* evTOp */   { stReqSent, { acScr, acNull, acNull } },   
    /* evTOm */   { stStopped, { acTlf, acNull, acNull } },   
    /* evRCRp */  { stOpened, { acSca, acTlu, acNull } },   
    /* evRCRm */  { stAckRcvd, { acScn, acNull, acNull } },   
    /* evRCA */   { stReqSent, { acScr, acNull, acNull } },   
    /* evRCN */   { stReqSent, { acScr, acNull, acNull } },   
    /* evRTR */   { stReqSent, { acSta, acNull, acNull } },   
    /* evRTA */   { stReqSent, { acNull, acNull, acNull } },   
    /* evRUC */   { stAckRcvd, { acScj, acNull, acNull } },   
    /* evRXJp */  { stReqSent, { acNull, acNull, acNull } },   
    /* evRXJm */  { stStopped, { acTlf, acNull, acNull } },   
    /* evRXR */   { stAckRcvd, { acNull, acNull, acNull } },   
    },   
    { /* stAckSent */   
    /* evUp */    { stNoChange, { acNull, acNull, acNull } },   
    /* evDown */  { stStarting, { acNull, acNull, acNull } },   
    /* evOpen */  { stAckSent, { acNull, acNull, acNull } },   
    /* evClose */ { stClosing, { acIrc, acStr, acNull } },   
    /* evTOp */   { stAckSent, { acScr, acNull, acNull } },   
    /* evTOm */   { stStopped, { acTlf, acNull, acNull } },   
    /* evRCRp */  { stAckSent, { acSca, acNull, acNull } },   
    /* evRCRm */  { stReqSent, { acScn, acNull, acNull } },   
    /* evRCA */   { stOpened, { acIrc, acTlu, acNull } },   
    /* evRCN */   { stAckSent, { acIrc, acScr, acNull } },   
    /* evRTR */   { stReqSent, { acSta, acNull, acNull } },   
    /* evRTA */   { stAckSent, { acNull, acNull, acNull } },   
    /* evRUC */   { stAckSent, { acScj, acNull, acNull } },   
    /* evRXJp */  { stAckSent, { acNull, acNull, acNull } },   
    /* evRXJm */  { stStopped, { acTlf, acNull, acNull } },   
    /* evRXR */   { stAckSent, { acNull, acNull, acNull } },   
    },   
    { /* stOpened */   
    /* evUp */    { stNoChange, { acNull, acNull, acNull } },   
    /* evDown */  { stStarting, { acTld, acNull, acNull } },   
    /* evOpen */  { stOpened, { acNull, acNull, acNull } },   
    /* evClose */ { stClosing, { acTld, acIrc, acStr } },   
    /* evTOp */   { stNoChange, { acNull, acNull, acNull } },   
    /* evTOm */   { stNoChange, { acNull, acNull, acNull } },   
    /* evRCRp */  { stAckSent, { acTld, acScr, acSca } },   
    /* evRCRm */  { stReqSent, { acTld, acScr, acScn } },   
    /* evRCA */   { stReqSent, { acTld, acScr, acNull } },   
    /* evRCN */   { stReqSent, { acTld, acScr, acNull } },   
    /* evRTR */   { stStopping, { acTld, acZrc, acSta } },   
    /* evRTA */   { stReqSent, { acTld, acScr, acNull } },   
    /* evRUC */   { stOpened, { acScj, acNull, acNull } },   
    /* evRXJp */  { stOpened, { acNull, acNull, acNull } },   
    /* evRXJm */  { stStopping, { acTld, acIrc, acStr } },   
    /* evRXR */   { stOpened, { acSer, acNull, acNull } },   
    }   
};   
   
//Code 종류들   
#define CODE_CONF_REQ   1   
#define CODE_CONF_ACK   2   
#define CODE_CONF_NAK   3   
#define CODE_CONF_REJ   4   
#define CODE_TERM_REQ   5   
#define CODE_TERM_ACK   6   
#define CODE_CODE_REJ   7   
#define CODE_PROTO_REJ  8   
#define CODE_ECHO_REQ   9   
#define CODE_ECHO_REP   10   
#define CODE_DISCARD_REQ    11   

struct LCP_options {   
    int accm; 
    unsigned char magic[4]; 
    int pcomp; 
    int accomp;   
} lcp_option;    
   
//------------------------------------------------------------------------------------------------------------------------------------------------   
   
int lcp_EchoReq(unsigned char*arr) {   
    arr[0] = 0xC0;   
    arr[1] = 0x21;   
    arr[2] = 0x09;   
    arr[3] = 0x00;   
    arr[4] = 0x00;   
    arr[5] = 0x08;   
    arr[6] = 0x01;   
    arr[7] = 0x02;   
    arr[8] = 0x03;   
    arr[9] = 0x04;   
    makefcs(arr, 10);   
    return 12;   
}   
 
int ipcp_ConReq(unsigned char*arr) {   
    arr[0] = 0x80;   
    arr[1] = 0x21;   
    arr[2] = 0x01;   
    arr[3] = 0x01;   
    arr[4] = 0x00;   
    arr[5] = 0x10;   
    arr[6] = 0x02;   
    arr[7] = 0x06;   
    arr[8] = 0x00;   
    arr[9] = 0x2D;   
    arr[10] = 0x0F;   
    arr[11] = 0x01;   
    arr[12] = 0x03;   
    arr[13] = 0x06;   
    arr[14] = 0x0A;   
    arr[15] = 0x01;   
    arr[16] = 0x03;   
    arr[17] = 0x0A;   
    makefcs(arr, 18);   
    return 20;   
}   

int lcp_TerReq(unsigned char*arr) {  
	arr[0] = 0xC0;   
    arr[1] = 0x21;   
    arr[2] = 0x05;   
    arr[3] = 0x00;   
    arr[4] = 0x00;   
    arr[5] = 0x04;   
    makefcs(arr, 6);   
    return 8;
} 

   
int lcp_ConReq(unsigned char*arr){ 
    int cnt=5; 
    //arr[0] = 0xFF;   
    //arr[1] = 0x03;   
    arr[0] = 0xC0;      
    arr[1] = 0x21;   
    arr[2] = 0x01;       
    arr[3] = 0x01;     
    arr[4] = 0x00;     
    arr[5] = 0x00;   
    
    //ACCM option(Asynchronous Control Character Map)
    if(lcp_option.accm==1){ 
        arr[++cnt]=0x02; 
        arr[++cnt]=0x06; 
        arr[++cnt]=0x00; 
        arr[++cnt]=0x00; 
        arr[++cnt]=0x00; 
        arr[++cnt]=0x00; 
    } 

    //Magic Number option
    arr[++cnt]=0x05;
    arr[++cnt]=0x06; 
    arr[++cnt]=0x01; 
    arr[++cnt]=0x02; 
    arr[++cnt]=0x03; 
    arr[++cnt]=0x04; 
 
    //PComp option(Protocol Compression)
    if(lcp_option.pcomp==1){ 
        arr[++cnt]=0x07; 
        arr[++cnt]=0x02; 
    } 

    //ACComp option(Address and Control Field Compression)
    if(lcp_option.accomp==1){ 
        arr[++cnt]=0x08; 
        arr[++cnt]=0x02; 
    } 
    arr[5] = cnt-1; 
    makefcs(arr, ++cnt); 
    return (cnt+2); 
} 
   
void Constructing_Options(unsigned char*arr){ 
    int i; 
    for(i=6;i<arr[5]+2;i++){ 
        if(arr[i]==0x02){ 
            lcp_option.accm=1; 
            i+=5; 
        } 

        if(arr[i]==0x05){ 
            int a; 
            for(a=0;a<4;a++){ 
                lcp_option.magic[a]=arr[i+2+a]; 
            } 
            i+=5; 
        }

        if(arr[i]==0x07){ 
            lcp_option.pcomp=1; 
            i+=1; 
        } 

        if(arr[i]==0x08){ 
            lcp_option.accomp=1; 
            i+=1; 
        } 
    } 
} 
   
int add(unsigned char* arr, int res) {   
    int k, i;   
    for(k=0; k<res; k++) {   
        if (arr[k] < 0x20 || arr[k] == 0x7E || arr[k] == 0x7D) {  
            arr[k] ^= 0x20;   
            for (i=res; i>k; i--)   
                arr[i] = arr[i-1];   
            arr[k] = 0x7D;   
            res++;   
            k++;   
        }   
    }   
    for(k=res; k>0; k--){
        arr[k] = arr[k-1];   
    }
    arr[0] =  0x7E;
    arr[res+1] =  0x7E;
    res += 2;   
    return res;   
}   
   
   
   
void Sending(unsigned char*arr, int code){
    int snd;   
    int len;  
    int res3; 
    arr[2]=code;   
    len =(arr[5])+2;   
    int i;  
    if(code==10){
        
        //printf("\n\n\n\n\n\n\n\n\nECHO!!!!!!!!!!!!!!!!!!!!\n");
         /*
        for(i=0;i<len;i++){   
            printf("%X\t",arr[i]);   
        }
        */
        arr[6]=0x01;
        arr[7]=0x02;
        arr[8]=0x03;
        arr[9]=0x04;
        /*
        printf("\nEcho Rearraged\n");
        for(i=0;i<len;i++){   
            printf("%X\t",arr[i]);   
        }
        */
    } 
    for(i=0;i<len;i++){   
        buf2[i+2]=arr[i];   
    }   
    buf2[0]=0xff;   
    buf2[1]=0x03;   
    len+=2;   
    /*
    printf("\nSENDING~~(proto)\n");   
    for(i=0;i<len;i++){   
        printf("%X\t",buf2[i]);   
    } 
    */  
    //printf("\n\n");   
    makefcs(buf2,len);   
    len+=2;   
    //usleep(500000);   
    len = add(buf2,len);   
    printf("SEND : ");   
    for(i=0;i<len;i++){   
        printf("%X\t",buf2[i]);   
    }   
    printf("\n"); 
    if(code==1){
        res3 = lcp_ConReq(buf3);   
        res3 = add(buf3, res3);   
        snd = write(fd,buf3,res3);
        return;
    }  
    snd = write(fd,buf2,len); 
      
}   
   
   
   
   
void execute_Actions(unsigned char*arr, enum xcp_action act){   
    switch(act){   
        case acNull ://acnull
            printf("act Null\n");   
        break;   
        case acIrc ://acirc
            printf("Initialize Restart Count\n");   
        break;   
        case acScr ://acscr   
            printf("Sending CODE_CONF_REQ\n");   
            Sending(arr, CODE_CONF_REQ);   
        break;   
        case acTls ://tls
            printf("This Layer Started\n");   
        break;   
        case acTlf ://tlf   
            printf("This Layer Finished\n");
        break;   
        case acStr ://str   
            printf("Sending CODE_TERM_REQ\n");   
            Sending(arr,CODE_TERM_REQ);   
        break;   
        case acSta ://sta   
            printf("Sending CODE_TERM_ACK\n");   
            Sending(arr,CODE_TERM_ACK);   
        break;   
        case acSca ://sca   
            printf("Sending CODE_CONF_ACK\n");   
            Sending(arr,CODE_CONF_ACK);   
        break;   
        case acScn ://scn   
            printf("Sending CODE_CONF_NAK\n");   
            Sending(arr,CODE_CONF_NAK);   
            Sending(arr,CODE_CONF_REJ);   
        break;   
        case acScj ://scj   
            printf("Sending CODE_CODE_REJ\n");      
            Sending(arr,CODE_CODE_REJ);   
        break;   
        case acTld ://tld
            printf("This Layer Down\n");   
        break;   
        case acTlu ://tlu   
            printf("This Layer Up\n");
        break;   
        case acZrc ://zrc
            printf("Zero Restart Count\n");   
        break;   
        case acSer ://ser   
            printf("\nSending CODE_ECHO_REP\n");   
            Sending(arr,CODE_ECHO_REP);   
        break;   
    }   
   
}   
   
   
   
   
   
   
   
   
   
int whatevent(unsigned char*arr, int present_state,int res){   
    int event=0;   
    switch(arr[2]){   
        case 0x01 : event=6;printf("\nRECEIVED : Con-Req\n");   
        break;   
        case 0x02 : event=8;printf("\nRECEIVED : Con-Ack\n");   
        break;   
        case 0x03 : event=9;printf("\nRECEIVED : Con-Nak\n");   
        break;   
        case 0x04 : event=9;printf("\nRECEIVED : Con-Rej\n");   
        break;   
        case 0x05 : event=10;printf("\nRECEIVED : Ter-Req\n");   
        break;   
        case 0x06 : event=11;printf("\nRECEIVED : Ter-Ack\n");   
        break;   
        case 0x07 : event=13;printf("\nRECEIVED : Code-Rej\n");   
        break;   
        case 0x08 : event=13;printf("\nRECEIVED : Proto-Rej\n");   
        break;   
        case 0x09 : event=15;printf("\nRECEIVED : Echo-Req\n");   
        break;   
        case 0x0A : event=15;printf("\nRECEIVED : Echo-Rep\n");   
        break;   
        case 0x0B : event=15;printf("\nRECEIVED : Discard-Req\n");   
        break;   
    }   
    printf("Event : %d\n",event);   
    printf("State : %d\n",present_state);   
    int tmp_state;   
    tmp_state=present_state;   
    present_state=ppp_dispatch[tmp_state][event].next;   
    printf("Next State: %d\n",present_state);   
    int i;
    /*   
    printf("\nBefore_Action!\n");   
    for (k=0; k<res; k++)   
        printf("%X\t", arr[k]);   
    printf("\n\n"); 
    */  
    for( i=0;i<3;i++){   
        if(ppp_dispatch[tmp_state][event].act[i] !=acNull){   
            printf("Action : %d  ",ppp_dispatch[tmp_state][event].act[i]);   
            execute_Actions(arr, ppp_dispatch[tmp_state][event].act[i]);   
        }   
    }   
    printf("\n\n"); 
    return present_state;   
}   
   
   
int get_rid(unsigned char*arr, int res) {   
    int k, i;   
    for (k=0; k<res; k++) {   
        if (arr[k] == 0x7D) {   
            if (arr[k+1] == (arr[k+1] | 0x20))   
                arr[k+1] -= 0x20;   
            else   
                arr[k+1] += 0x20;   
            for (i=k; i<res; i++)   
                arr[i] = arr[i+1];   
            res -= 1;   
        }   
    }   
    for (k=0; k<res-1; k++){
        arr[k] = arr[k+1];   
    }
    res -= 2;   
    return res;   
}   


   
int main(void){   
    int res, snd, k, res1, res2, res3, order;   
    struct termios oldtio, newtio;   
    int te=0;
   
    bzero(&buf, sizeof(buf));    //open serial device   
    fd = open(SERIALDEVICE, O_RDWR | O_NOCTTY, O_NONBLOCK);   
    if(fd<0){   
        perror(SERIALDEVICE);   
        exit(-1);   
    }       
    tcgetattr(fd, &oldtio);    
   
    bzero(&newtio, sizeof(newtio));    //serial device settings   
    newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;   
    newtio.c_iflag = IGNPAR | ICRNL;   
    
    tcflush(fd, TCIFLUSH);   
    tcsetattr(fd, TCSANOW, &newtio);   
    fflush(stdin);   
    fflush(stdout);   
        
    for(k=0; k<10000; k++){   
        buf[k] = 0x00;   
        buf2[k] = 0x00;   
        buf3[k] = 0x00;   
    }   
    for(k=0; k<5000; k++){   
        buf1[k] = 0x00;   
    }   

    tcflush(fd, TCIFLUSH);   
    tcflush(fd, TCIFLUSH);   
    tcflush(fd, TCIFLUSH);   

//------------------------------------------------------------------------------------------------------------------------------------------------   

    ppp_phase present_phase = phDead;   
    xcp_state present_state = stInitial;
    present_state = ppp_dispatch[present_state][0].next;
    order = 1;  
   // int options; 
   // options=0; 
    
    while(1)      //read or write serial device   
    {   
        fflush(stdin);   
        fflush(stdout);   
        res = read(fd, buf, 10000);   
        if (res != 0) {   
            /*   
            for (k=0; k<res; k++)   
                printf("%X\t", buf[k]);   
            printf("\n");   
            */   
            printf("STREAMING : ");   
            for (k=0; k<res; k++)   
                printf("%X\t", buf[k]);   
            printf("\n");
            if (buf[0] == 0x7E && buf[res-1] != 0x7E) {   
                while(buf[res-1] != 0x7E) {   
                    res1 = read(fd, buf1, 5000);   
                    if (res1 != 0) {   
                        if (buf1[0] != 0x7E) {   
                            for (k=res; k-res<res1; k++)   
                                buf[k] = buf1[k-res];   
                            res += res1;   
                        }   
                    }   
                    res1=0;   
                }   
            }   
            for (k=0; k<res; k++)   
                tmp2_buf[k]=buf[k];   
            int res4;  
            res4=res;  
               
            int i;   
            int st=0;   
            for(i=1; i<res4;i++){   
                if(tmp2_buf[i]==0x7E){   
                    int a ;  
                    for(a=st;a<=i;a++){   
                        tmp_buf[a-st]=tmp2_buf[a];   
                    }   
                    printf("\nORGANIZED : ");  
                    for (k=0; k<(i-st+1); k++)   
                        printf("%X\t", tmp_buf[k]);   
                    printf("\n");   
                    int len;  
                    len=i-st+1;  
                    if (tmp_buf[0] == 0x7E && len !=1) {   
                        len = get_rid(tmp_buf, len);   
                        if (checkfcs(tmp_buf, len-2) == 1) {  //CRC check   
                            if (tmp_buf[0] == 0xFF) {  //HDLC address & control field handle   
                                for (k=0; k<len-2; k++)   
                                    tmp_buf[k] = tmp_buf[k+2];   
                            len -= 2;   
                            }   
                       /*
                    printf("\nMIDTEST\n");   
                    for (k=0; k<len; k++)   
                        printf("%X\t", tmp_buf[k]);   
                    printf("\n\n");   
                    */





                    /*   
                     *   
                     *LCP Protocol   
                     *   
                     */   
                    if (tmp_buf[0] == 0xC0 && tmp_buf[1] == 0x21) { 
                        Constructing_Options(tmp_buf);
                       // printf("ORDER : %d\n",order);
                       if(tmp_buf[2] == 0x06)
						te=1;
                        if(order==1){
                            res3 = lcp_ConReq(buf3);
                            printf("State:%d\n",present_state);   
                            for(i=0;i<3;i++){   
                                if(ppp_dispatch[present_state][2].act[i] !=acNull){   
                                    printf("Action : %d  ",ppp_dispatch[present_state][2].act[i]);   
                                    execute_Actions(buf3, ppp_dispatch[present_state][2].act[i]);   
                                }   
                            }
                            //snd = write(fd,buf3,res3); 
                            present_state = ppp_dispatch[present_state][2].next;
                            order++; 
                            printf("Next State:%d\n",present_state);   

                        }
                       // if(tmp_buf[5]==0x0A) 
                       //     options=1; 
                         
                        printf("LCP:");   
                        for (k=0; k<len; k++)   
                           printf("%X\t", tmp_buf[k]);   
                        printf("\n");   
                        present_state=whatevent(tmp_buf, present_state,len);   
                        if(present_state==9 && present_phase==0){   
                            present_phase++;   
                        }    
                        //printf("\nbuf2 size:%d\n",how_long(buf2));   
                        printf("\nPhase : %d\n\n",present_phase); 
                        if(present_phase==1)
                            printf("(Establish)Phase Completed : %d\n",present_phase);
                        if(present_state==5){   
                            printf("\nTERMINATE!!\n");   
                            present_state=0;   
                            present_phase=0;
                            present_state = ppp_dispatch[present_state][0].next;
                            order=0;

                        }   
                        order++;                          
                    }   
 
 
 
 
                    /*   
                     *   
                     *IPCP Protocol   
                     *   
                     */   
                    else if (tmp_buf[0] == 0x80 && tmp_buf[1] == 0x21) {   
                        if(present_phase==0){   
                            printf("\nNETWORK NOT OPENED!\n");   
                        }   
  
                        else if(present_phase==1){   
                            printf("IPCP:");
                              
                            for(k=0; k<len; k++)   
                                printf("%X\t", tmp_buf[k]);   
                            printf("\n");
                            tmp_buf[2]=0x02;   
                            Sending(tmp_buf, 2);
                            present_phase+=2;
                            printf("\n(Network)Phase Completed : %d\n\n",present_phase); 
                        }      
                    }   
                }   
                else {   //CRC error   
                    printf("\nCRC ERROR!!!!\n");   
                }   
                    }   
                    st=i;  
                }    
            }  
        }  
        if (order == 3) {   
            res3 = lcp_EchoReq(buf3);   
            res3 = add(buf3, res3);   
            snd = write(fd,buf3,res3);   
            res3 = ipcp_ConReq(buf3);   
            res3 = add(buf3, res3);   
            snd = write(fd,buf3,res3);   
            order++;   
        }   
		if(present_phase==3){
			usleep(5000000);
			res3 = lcp_TerReq(buf3);   
            res3 = add(buf3, res3);   
            snd = write(fd,buf3,res3);
            present_phase++;
            printf("TERMINATED!!!\n");			
            usleep(5000000);
			
		}
		if(present_phase==4 &&te==1)
			break;

        res = 0;   
        res2 = 0;   
        res3 = 0;   
		



    } //while end   
    tcsetattr(fd,TCSANOW, &oldtio);       
    return 0;   
}









