#ifndef _IOBUS_HDLC_H_
#define _IOBUS_HDLC_H_

#include <stdbool.h>

#define HDLC_TIMEOUT_L          	10000     	//10ms
//#define HDLC_TIMEOUT_S          	500        	//500us 68302不行
#define HDLC_TIMEOUT_S          	1000       	//1ms
#define HDLC_BROADCAST_TIMEINTERVAL (50*1000)  //100ms

typedef struct
{
	unsigned char cmd;			//HDLC通信命令
	unsigned char len;			//HDLC命令长度
	unsigned char cont[48];		//HDLC命令内容
}comm_cmd_t;

typedef struct
{
	unsigned int comm_cnt;		//HDLC通信次数计数
	unsigned int timeout_cnt;	//HDLC通信超时计数
	unsigned int err_cnt;		//HDLC通信错误计数
	bool timeout_flag;
	bool err_flag;
}comm_stat_t;

typedef struct 
{
	bool plug;					//插拔状态
	unsigned char addr; 		//模块地址
	unsigned char type;			//模块类型（版码）
	comm_cmd_t comm_cmd;		//通信命令
	comm_stat_t comm_stat;		//通信状态
}module_info_t;

extern module_info_t *module;

extern void *hdlc_comm_func(void *arg);
#endif
