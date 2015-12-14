#ifndef _IOBUS_COMMON_H_
#define _IOBUS_COMMON_H_

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/time.h>

#ifdef USER_DEBUG
#define USER_DBG(fmt, arg...) \
	do { \
		printf("(USER_DBG)%s[%d](%s):", __FILE__, __LINE__, __FUNCTION__);\
		printf(fmt, ##arg); \
	}while(0)
#else
#define USER_DBG(fmt, arg...)
#endif
 
#define DEV_NAME			"/dev/iobus"

#define OLTEST_MODULE_NUM	1			//开环测试模块数量
#define CLTEST_MODULE_NUM	2			//闭环测试模块数量
#define AGTEST_MODULE_NUM	64			//老化测试模块数量


#define CHLD_PACK_LEN_L		64			//子包长度64，适用于开环和闭环测试
#define CHLD_PACK_LEN_S		16			//子包长度16，适用与老化测试

#define START				true    
#define STOP				false

#define HDLC_CH1			0
#define HDLC_CH2			1
#define HDLC_CH_ALL			2

#define SLAVE				0
#define MASTER				1

#define IOBUS_IOC_MAGIC             'w'
#define IOBUS_IOC_RUN_STAT          _IOW(IOBUS_IOC_MAGIC, 1, int)   //设置主从
#define IOBUS_IOC_CH_SEL            _IOW(IOBUS_IOC_MAGIC, 2, int) //通道选择
#define IOBUS_IOC_LED_STAT			_IOW(IOBUS_IOC_MAGIC, 3, int)
#define IOBUS_IOC_MAXNR             4

typedef struct 
{
	int log_fd;
	int dev_fd;									//设备文件描述符
	int server_fd;								//下位机socket描述符
	int client_fd;								//上位机socket描述符
	pthread_t hdlc_comm_tid;					//通信线程id
	volatile bool hdlc_ch_change_flag;			//HDLC切换通道标志
	pthread_mutex_t hdlc_mutex;					//线程锁用于切换通道时的数据完整性保护
	pthread_mutex_t hdlc_ch_change_mutex;		//HDLC切换通道互斥锁
	struct itimerval hdlc_itimerval;			//HDLC轮询定时器，用于切换通道和广播
	pthread_t sig_tid;
	sigset_t sig_mask; 
	unsigned int host_cmd;						//上位机网络命令
	unsigned int test_cmd;						//下位机测试状态，开环、闭环、老化，用于返给上位机
	unsigned int net_child_packet_size;			//网络自定义协议子包长度
	unsigned int net_timeout;					//NET通讯超时时间
	unsigned int module_num;					//模块数量
	unsigned int hdlc_timeout;					//HDLC通讯超时时间
	unsigned int hdlc_ch;						//HDLC通道
	unsigned int hdlc_stat;						//主从，主发时钟，从接时钟
	volatile bool run_stat;						//IOBUS运行状态
	volatile bool get_data_flag;				//上位机获取数据标志
	unsigned char hdlc_broadcast_buf[5];		//广播数据
}global_param_t;

extern global_param_t glb;

extern void set_timeval(struct timeval *tv, long sec, int usec);
extern bool system_init(void);
#endif
