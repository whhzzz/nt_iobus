#ifndef _IOBUS_NET_H_
#define _IOBUS_NET_H_

#include <stdbool.h>

#define TCP_PORT		7000
#define NET_TIMEOUT		5000		//500ms

#define CMD_OPENLOOP    	0xAAAAAAAA
#define CMD_CLOSELOOP   	0xBBBBBBBB
#define CMD_AGING      		0xCCCCCCCC
#define CMD_AGING_LONGFRAME	0xA5A5A5A5
#define CMD_STOP	   		0xDDDDDDDD
#define CMD_CHANGECH		0xEEEEEEEE
#define CMD_GETDATA			0xFFFFFFFF
#define CMD_LEN				4


extern unsigned char net_rbuf[1500];
extern unsigned char net_sbuf[1500];

extern bool create_server_socket(void);
extern void net_data_recv_loop(void);
extern void hdlc_ch_change_sig_func(int signo);
#endif

