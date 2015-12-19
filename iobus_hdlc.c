#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "iobus_hdlc.h"
#include "iobus_crc.h"
#include "iobus_common.h"
#include "iobus_net.h"


static int fill_hdlc_send_buf(module_info_t *module_head, int module_num, unsigned char *hdlc_sbuf);
static void parse_hdlc_recv_data(module_info_t *module_head, int module_num, unsigned char *hdlc_rbuf, int hdlc_rlen);
static void fill_net_send_packet(module_info_t *module_head, int module_num, unsigned char *hdlc_rbuf, int len, unsigned char *net_sbuf);
static void hdlc_comm_thread_cleanup(void *arg);

module_info_t *module;


/**
 * @brief	填充HLDC发送缓存，该数据发送给模块，具体命令格式参照《EDPF模块指令集说明》
 * @param	*module_head	模块信息结构体数组首地址
 * @param	*module_num		模块信息结构体数组索引
 * @param	*hdlc_sbuf		HDLC发送缓存
 * @return	int类型，通过HDLC发送的数据长度				
 */

int fill_hdlc_send_buf(module_info_t *module_head, int module_num, unsigned char *hdlc_sbuf)
{
	unsigned short crc_res = 0;
	int hdlc_slen = (module_head+module_num)->comm_cmd.len+4;
	hdlc_sbuf[0] = (module_head+module_num)->addr;
	hdlc_sbuf[1] = (module_head+module_num)->comm_cmd.cmd;
	memcpy(&hdlc_sbuf[2], (module_head+module_num)->comm_cmd.cont, (module_head+module_num)->comm_cmd.len);
	crc_res = crc_calc(hdlc_sbuf, hdlc_slen-2);
	hdlc_sbuf[hdlc_slen-2] = crc_res & 0xff;
	hdlc_sbuf[hdlc_slen-1] = (crc_res >> 8) & 0xff;
	return hdlc_slen;
}


/**
 * @brief	
 * @param	*module_head	模块信息结构体数组首地址
 * @param	*module_num		模块信息结构体数组索引
 * @param	*hdlc_rbuf		HDLC接收缓存
 * @param   hdlc_rlen		HDLC接收数据长度
 * @param   timeout_flag	HDLC超时标志
 */

void parse_hdlc_recv_data(module_info_t *module_head, int module_num, unsigned char *hdlc_rbuf, int hdlc_rlen)
{
	if ((module_head+module_num)->plug == true)
	{
		(module_head+module_num)->comm_stat.comm_cnt++;
		if ((module_head+module_num)->comm_stat.timeout_flag == true)
		{
			(module_head+module_num)->comm_stat.timeout_cnt++;
		}
		else
		{
			(module_head+module_num)->comm_stat.err_flag = false;
			unsigned short crc_res = crc_calc(hdlc_rbuf, hdlc_rlen-2);
			if ((hdlc_rbuf[hdlc_rlen-1] != ((crc_res >> 8) & 0xff)) || (hdlc_rbuf[hdlc_rlen-2] != (crc_res & 0xff)))
			{
				(module_head+module_num)->comm_stat.err_cnt++;
				(module_head+module_num)->comm_stat.err_flag = true;
			}
			else
			{
				if (glb.test_cmd == CMD_OPENLOOP || glb.test_cmd == CMD_CLOSELOOP)
				{
					if ((hdlc_rbuf[0] != (module_head+module_num)->addr) || (hdlc_rbuf[2] != (module_head+module_num)->type))
					{
						(module_head+module_num)->comm_stat.err_cnt++;
						(module_head+module_num)->comm_stat.err_flag = true;
					}
				}
			}
		}
	}
	else
	{
		if ((module_head+module_num)->comm_stat.timeout_flag == true) //如果认为卡件未插，并且仍旧通讯超时
		{
			hdlc_rbuf[2] = 0;										  //此时向上位机返回一个为0的版码值表明没有卡件,手动添加hdlc_rbuf对应版码值的那一位
		}
		else														  //否则，认为卡件未插，但该此通讯通上了，说明卡件在此次通讯之前插入，设置卡件插入状态标志
		{
			if ((hdlc_rbuf[0] == (module_head+module_num)->addr) && (hdlc_rbuf[3] == 0x1))
			{
				(module_head+module_num)->plug = true;
			}
		}
	}
}

/**
 * @brief  填充网络发送数据缓存	
 * @param	*module_head	模块信息结构体数组首地址
 * @param	*module_num		模块信息结构体数组索引
 * @param	*hdlc_rbuf		HDLC接收缓存
 * @param   hdlc_rlen		HDLC接收数据长度
 * @param   *net_sbuf		网络发送缓存
 */

void fill_net_send_packet(module_info_t *module_head, int module_num, unsigned char *hdlc_rbuf, int hdlc_rlen, unsigned char *net_sbuf)
{
	parse_hdlc_recv_data(module_head, module_num, hdlc_rbuf, hdlc_rlen);
	net_sbuf[0] = glb.test_cmd & 0xff;
	net_sbuf[1] = (glb.test_cmd >> 8) & 0xff;
	net_sbuf[2] = (glb.test_cmd >> 16) & 0xff;
	net_sbuf[3] = (glb.test_cmd >> 24) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+4] = (module_head+module_num)->comm_stat.comm_cnt & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+5] = ((module_head+module_num)->comm_stat.comm_cnt >> 8) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+6] = ((module_head+module_num)->comm_stat.comm_cnt >> 16) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+7] = ((module_head+module_num)->comm_stat.comm_cnt >> 24) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+8] = (module_head+module_num)->comm_stat.timeout_cnt & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+9] = ((module_head+module_num)->comm_stat.timeout_cnt >> 8) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+10] = ((module_head+module_num)->comm_stat.timeout_cnt >> 16) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+11] = ((module_head+module_num)->comm_stat.timeout_cnt >> 24) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+12] = (module_head+module_num)->comm_stat.err_cnt & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+13] = ((module_head+module_num)->comm_stat.err_cnt >> 8) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+14] = ((module_head+module_num)->comm_stat.err_cnt >> 16) & 0xff;
	net_sbuf[module_num*glb.net_child_packet_size+15] = ((module_head+module_num)->comm_stat.err_cnt >> 24) & 0xff;
	if ((module_head+module_num)->comm_stat.timeout_flag == false)
	{
		if (glb.test_cmd == CMD_OPENLOOP || glb.test_cmd == CMD_CLOSELOOP)
		{
			if ((hdlc_rlen - 6) > 0)
			{
				net_sbuf[module_num*glb.net_child_packet_size+16] = hdlc_rlen - 6;
				memcpy(&net_sbuf[module_num*glb.net_child_packet_size+17], &hdlc_rbuf[3], hdlc_rlen - 6);	
			}
			else
			{
				memset(&net_sbuf[module_num*glb.net_child_packet_size+16], 0, glb.net_child_packet_size-16);	//一开始没处理这个清零，因为对于一般卡件输出命令，比如DO全路输出，其返回命令为ADDR 0x00 ID_MODULE 0x01加crc一共6个字节，上面if判断不到，net_sbuf[16]以后保持之前数据不被覆盖，而上位机APP利用net_sbuf[16]判断是否需要将命令从全路输出切回全路扫描，当net_sbuf[16]为0时，即cmdContLength为0时，切回全路扫描，正因为net_sbuf[16]没有重新填充导致命令一直为全路输出，这样会回读不到端子状态
			}
		}
		else
		{
			memcpy(&net_sbuf[module_num*glb.net_child_packet_size+16], &hdlc_rbuf[2], 1);	
		}
	}
}

/**
 * @brief 线程处理函数，负责HDLC循环发送和接收
 */
void *hdlc_comm_func(void *arg)
{
	int num = 0;
	int ret = 0;
	fd_set fds;
	struct timeval tv = {0, 0};
	struct timeval tv1 = {0, 0};
	struct tm *tm = NULL;
	int ch_num = 0;
	int hdlc_slen = 0;
	int hdlc_rlen = 0;
	int net_slen = 0;
	unsigned char hdlc_sbuf[64];
	unsigned char hdlc_rbuf[64];
	char log_buf[200];
	pthread_cleanup_push(hdlc_comm_thread_cleanup, NULL);	//设置线程清理函数
	while (glb.run_stat == START)
	{
		num = 0;
		while (num < glb.module_num)
		{
			if (!(((module+num)->plug == false) && (glb.test_cmd == CMD_AGING_LONGFRAME))) //如果进入长帧扫描状态，且卡件还未插就不去扫描当前地址了

			{
				memset(hdlc_sbuf, 0, sizeof(hdlc_sbuf));
				hdlc_slen = fill_hdlc_send_buf(module, num, hdlc_sbuf);
				set_timeval(&tv, 0, glb.hdlc_timeout);	
				FD_ZERO(&fds);
				FD_SET(glb.dev_fd, &fds);
				pthread_mutex_lock(&glb.hdlc_mutex);
				if ((ret = select(glb.dev_fd+1, NULL, &fds, NULL, NULL)) <= 0)
				{
					pthread_mutex_unlock(&glb.hdlc_mutex);
					continue;
				}
				else 
				{
					if (FD_ISSET(glb.dev_fd, &fds))
					{
						for (;;)
						{
							if (write(glb.dev_fd, hdlc_sbuf, hdlc_slen) == hdlc_slen)
							{
								break;
							}
						}
					}
				}		
				memset(hdlc_rbuf, 0, sizeof(hdlc_rbuf));
				set_timeval(&tv, 0, glb.hdlc_timeout);	
				FD_ZERO(&fds);
				FD_SET(glb.dev_fd, &fds);
				if ((ret = select(glb.dev_fd+1, &fds, NULL, NULL, &tv)) < 0)
				{
					perror("Select for read HDLC receive buffer.\n");
					pthread_mutex_unlock(&glb.hdlc_mutex);
					continue;
				}
				else if (ret == 0)
				{
					USER_DBG("Wait for IO module return HDLC data timeout.\n");
					(module+num)->comm_stat.timeout_flag = true;	
				}
				else
				{
					for (;;)
					{
						if ((hdlc_rlen = read(glb.dev_fd, hdlc_rbuf, sizeof(hdlc_rbuf))) != -1)
						{
							(module+num)->comm_stat.timeout_flag = false;	
							break;
						}
					}
				}	
				pthread_mutex_unlock(&glb.hdlc_mutex);
			}
			pthread_mutex_lock(&glb.hdlc_ch_change_mutex);
			if (glb.hdlc_ch_change_flag == false)
			{
				fill_net_send_packet(module, num, hdlc_rbuf, hdlc_rlen, net_sbuf);		
			}	
			pthread_mutex_unlock(&glb.hdlc_ch_change_mutex);
			num++;
			gettimeofday(&tv1, NULL);
			tm = localtime(&tv1.tv_sec);
			if (((module+num)->plug == true) && (glb.test_cmd == CMD_AGING || glb.test_cmd == CMD_AGING_LONGFRAME))
			{
				if (glb.hdlc_ch == 1)
				{
					ch_num = 1;
				}
				else if (glb.hdlc_ch == 0)
				{
					ch_num = 2;
				}
				else 
				{
					ch_num = 3;
				}
				if ((module+num)->comm_stat.timeout_flag == true)
				{
					sprintf(log_buf, "addr(%d),channel(%d),comm_cnt(%d),timeout,%d%02d%02d-%02d:%02d:%02d:%ld", num, ch_num, (module+num)->comm_stat.comm_cnt, 1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tv1.tv_usec/1000) ; 
					write(glb.log_fd, log_buf, strlen(log_buf));
					write(glb.log_fd, "\n", 1);
				}
				if ((module+num)->comm_stat.err_flag == true)
				{
					sprintf(log_buf, "addr(%d),channel(%d),comm_cnt(%d),error,%d%02d%02d-%02d:%02d:%02d:%ld", num, ch_num, (module+num)->comm_stat.comm_cnt, 1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tv1.tv_usec/1000) ; 
					write(glb.log_fd, log_buf, strlen(log_buf));
					write(glb.log_fd, "\n", 1);
				}
			}
		}
		if (glb.get_data_flag == true)
		{
			FD_ZERO(&fds);
			FD_SET(glb.client_fd, &fds);
			set_timeval(&tv, 0, glb.net_timeout);
			net_slen = num*glb.net_child_packet_size + 4;
			if (select(glb.client_fd+1, NULL, &fds, NULL, &tv) > 0) 
			{
				if (FD_ISSET(glb.client_fd, &fds))
				{
					ret = send(glb.client_fd, net_sbuf, net_slen, 0);
				}
			}
			pthread_mutex_lock(&glb.hdlc_mutex);
			glb.get_data_flag = false;
			pthread_mutex_unlock(&glb.hdlc_mutex);
		}
	}
	pthread_cleanup_pop(1);
	pthread_exit(NULL);
}

void hdlc_comm_thread_cleanup(void *arg)
{
	free(module);
	module = NULL;	
	glb.hdlc_ch = HDLC_CH1;
	pthread_mutex_lock(&glb.hdlc_mutex);
    ioctl(glb.dev_fd, IOBUS_IOC_CH_SEL, glb.hdlc_ch);
    pthread_mutex_unlock(&glb.hdlc_mutex);
}

