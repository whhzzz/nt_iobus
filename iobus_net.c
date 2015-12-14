#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h> //S_IRWXU
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "iobus_net.h"
#include "iobus_common.h"
#include "iobus_hdlc.h"

unsigned char net_rbuf[1500];
unsigned char net_sbuf[1500];
char old_log_path[200] = {0};
char new_log_path[200] = {0};

static void parse_net_recv_packet(unsigned char *buf, int len);
/**
 * @brief  创建服务器端socket，监听端口号:7000, 本机所有IP地址
 * @return 创建成功返回ture，否则返回false
 */
/*void hdlc_ch_change_sig_func(int signo)
{
	switch(signo)
	{
		case SIGALRM:
			printf("fucnk!\n");
			pthread_mutex_lock(&glb.hdlc_ch_change_mutex);
			glb.hdlc_ch_change_flag = false;
			pthread_mutex_unlock(&glb.hdlc_ch_change_mutex);
			pthread_cond_signal(&glb.hdlc_ch_change_wait_cond);
			break;
	}
}*/

bool create_server_socket()
{
	struct sockaddr_in server_addr;
    if ((glb.server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
		perror("Create server socket failed: ");
        return false;
    }
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(glb.server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
		perror("Bind server socket failed: ");
	    close(glb.server_fd);
		return false;
    }
    if (listen(glb.server_fd, 5) == -1)
    {
        perror("Set server listen falied: ");
	    close(glb.server_fd);
		return false;	    
    }
	return true;
}

/**
 * @brief  	利用select非阻塞方式，多路同时检测是否有新的客户端连接以及连接的
 *			客户端是否有数据可读取，当有网络数据时获取数据并解析网络命令，创
 *			建线程进行HDLC通信
 * 
 */

void net_data_recv_loop()
{
	int ret = 0;
	int max_fd = glb.server_fd;
	bool connected = false;
	struct sockaddr_in client_addr;
	int addr_len = sizeof(client_addr);
	int net_rlen = 0;
	struct timeval tv = {0, 0};
	fd_set fds;
	while(1)
    {
  		FD_ZERO(&fds);
 		FD_SET(glb.server_fd, &fds);
	    if (connected == true)
        {
	        FD_SET(glb.client_fd, &fds);
        }
		set_timeval(&tv, 0, glb.net_timeout); 	//设置网络超时时间500ms
        ret = select(max_fd+1, &fds, NULL, NULL, &tv);//&tv);
        if (ret < 0)							//select调用出错，跳出此次循环
        {
            perror("Select failed: ");
			continue;
        }
		else if (ret == 0)						//超时,分两种情况，一种是在没有任何连接的情况下等待连接超时，一种是等待接收客户端网络数据超时
	    {
			if (connected == false)				
			{
				USER_DBG("Wait for client connect...\n");
			}
			continue;
        }
        else
        {
			if (FD_ISSET(glb.server_fd, &fds))		//如果是服务器端socket可读，说明有新的client连接了
           	{
				glb.client_fd = accept(glb.server_fd, (struct sockaddr *)&client_addr, &addr_len);
				if (glb.client_fd == -1)			//TCP三次握手失败，无法获取client端socket描述符
 		        {
 	            	USER_DBG("Three-way handshake failed.\n");
                }
                else
                {
                   	USER_DBG("A new client[IP:%s] has been connected.\n", inet_ntoa(client_addr.sin_addr));
                    connected = true;			//设置已连接标志
					if (glb.client_fd > max_fd)
					{
						max_fd = glb.client_fd;
					}
                }
            }
            if (FD_ISSET(glb.client_fd, &fds))		//说明网络上有客户端数据可以读取了    
            {
            	net_rlen = recv(glb.client_fd, net_rbuf, sizeof(net_rbuf), 0);
                if (net_rlen > 0)					//正常接收数据
                {
  					parse_net_recv_packet(net_rbuf, net_rlen);
                }
                else if ((net_rlen < 0) && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))	//阻塞被打断
                {
                	continue;
                }
                else								//client端socket关闭,上位机可以在网络断开后重连，这时服务端认为一个新的客户端连接，覆盖掉上一次的client端socket描述符，为这次新的连接再accpet一次
                {
					connected = false;
				}
			}
		}
	}
}

/**
 * @brief 解析以太网命令，包括开环测试、闭环测试、老化测试，并根据不同命令设
 *        置系统参数
 * @param   *buf 以太网接收缓存
 * @param   *len 以太网接收长度
 */
	
void parse_net_recv_packet(unsigned char *buf, int len)
{
	int i = 0;
	int sec = 0;
	int min = 0;
	int hour = 0;
	int day = 0;
	int mon = 0;
	int year = 0;
	time_t time = 0;
	struct tm tm;
	struct timeval tv;
	char time_buf[30] = {0};
	char *log_dir = "/home/ftp/aging_test_log_files";
	char *log_path = "/home/ftp/aging_test_log_files/aging_test_log-";
	glb.host_cmd = buf[0] + (buf[1]<<8) + (buf[2]<<16) + (buf[3]<<24);	//以太网命令4bytes
	switch(glb.host_cmd)
	{
		case CMD_OPENLOOP:		//开环测试：子包大小64bytes；测试模块数量1；HDLC超时时间100ms
			glb.test_cmd = CMD_OPENLOOP;
			glb.net_child_packet_size = CHLD_PACK_LEN_L;
			glb.module_num = OLTEST_MODULE_NUM;
			glb.hdlc_timeout = HDLC_TIMEOUT_L;	
			break;
		case CMD_CLOSELOOP:		//闭环测试：子包大小64bytes；测试模块数量2，模块1对应输出模块，模块2对应输入模块；HDLC超时时间100ms
			glb.test_cmd = CMD_CLOSELOOP;
			glb.net_child_packet_size = CHLD_PACK_LEN_L;
			glb.module_num = CLTEST_MODULE_NUM;
			glb.hdlc_timeout = HDLC_TIMEOUT_L;
			break;
		case CMD_AGING:			//老化测试：子包大小8bytes；测试模块数量64；HDLC超时时间5ms
			glb.test_cmd = CMD_AGING;
			glb.net_child_packet_size = CHLD_PACK_LEN_S;
			glb.module_num = AGTEST_MODULE_NUM;
			glb.hdlc_timeout = HDLC_TIMEOUT_L;
			strcpy(old_log_path, log_path);
			memcpy(&sec, &buf[4], 4);
			memcpy(&min, &buf[8], 4);
			memcpy(&hour, &buf[12], 4);
			memcpy(&day, &buf[16], 4);
			memcpy(&mon, &buf[20], 4);
			memcpy(&year, &buf[24], 4);
			tm.tm_sec = sec;
			tm.tm_min = min;
			tm.tm_hour = hour;
			tm.tm_mday = day;
			tm.tm_mon = mon-1;
			tm.tm_year = year - 1900;
			time = mktime(&tm);
			tv.tv_sec = time;
			tv.tv_usec = 0;
			if (settimeofday(&tv, NULL) < 0)
			{
				printf("no permission, make sure login with root user\n");
			}	
			sprintf(time_buf, "%d%02d%02d-%02d%02d%02d", year, mon, day, hour, min, sec);
			strcat(old_log_path, time_buf);
			strcpy(new_log_path, old_log_path);
			if (opendir(log_dir) == NULL)
			{
				if (mkdir(log_dir, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
				{
					printf("Can't make dir for ftp record\n");
					return;
				}
			}
			chmod(log_dir, S_IRWXU | S_IRWXG | S_IRWXO);
			glb.log_fd = open(old_log_path, O_WRONLY|O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
			if (glb.log_fd == -1)
			{
				printf("Can't create aging test log file\n");
				return;
			}
			break;
		case CMD_GETDATA:
			pthread_mutex_lock(&glb.hdlc_mutex);
			glb.get_data_flag = true;
			pthread_mutex_unlock(&glb.hdlc_mutex);
			return;	
		case CMD_STOP:			//停止测试：需等待HDLC通信线程退出
			glb.run_stat = STOP;
			pthread_join(glb.hdlc_comm_tid, NULL);
			if (glb.test_cmd == CMD_AGING)
			{
				memcpy(&sec, &buf[4], 4);
				memcpy(&min, &buf[8], 4);
				memcpy(&hour, &buf[12], 4);
				memcpy(&day, &buf[16], 4);
				memcpy(&mon, &buf[20], 4);
				memcpy(&year, &buf[24], 4);
				sprintf(time_buf, "~%d%02d%02d-%02d%02d%02d.csv", year, mon, day, hour, min, sec);
				strcat(new_log_path, time_buf);
				rename(old_log_path, new_log_path);
				chmod(new_log_path, S_IRWXU | S_IRWXG | S_IRWXO);
			}
			return;
		case CMD_CHANGECH:		//HDLC通道选择: 
			if (glb.hdlc_ch == 2)				
			{
				glb.hdlc_ch = 0;
			}
			else if (glb.hdlc_ch == 0) 
			{
				glb.hdlc_ch = 1; //通道1
			}
			else
			{
				glb.hdlc_ch = 0; //通道2
			}
			pthread_mutex_lock(&glb.hdlc_ch_change_mutex);
			glb.hdlc_ch_change_flag = true;
			pthread_mutex_unlock(&glb.hdlc_ch_change_mutex);
			pthread_mutex_lock(&glb.hdlc_mutex);
			ioctl(glb.dev_fd, IOBUS_IOC_CH_SEL, glb.hdlc_ch);
			pthread_mutex_unlock(&glb.hdlc_mutex);
			return;
		default:
			USER_DBG("Undefined net command.\n");
			return;
	}
	if (glb.run_stat == STOP)
	{
		module = malloc(sizeof(module_info_t)*glb.module_num);			//根据测试类型创建模块数组
		memset(module, 0, sizeof(module_info_t)*glb.module_num);
	}
	for (i=0; i<glb.module_num; i++)								//填充各模块数据
	{
		if (glb.test_cmd == CMD_OPENLOOP || glb.test_cmd == CMD_CLOSELOOP)
		{
			(module+i)->plug = true;
			(module+i)->type = buf[4+i*glb.net_child_packet_size];
			(module+i)->addr = buf[5+i*glb.net_child_packet_size];
			(module+i)->comm_cmd.cmd = buf[6+i*glb.net_child_packet_size];
			(module+i)->comm_cmd.len = buf[7+i*glb.net_child_packet_size];
			memcpy((module+i)->comm_cmd.cont, &buf[8+i*glb.net_child_packet_size], (module+i)->comm_cmd.len);
		}
		else
		{
			if (glb.test_cmd == CMD_AGING)
			{
				(module+i)->plug = false;
				(module+i)->addr = i;
				(module+i)->comm_cmd.cmd = 0;
				(module+i)->comm_cmd.len = 0;
			}
		}
	}
	if (glb.run_stat == STOP)
	{
		glb.run_stat = START;
		pthread_create(&glb.hdlc_comm_tid, NULL, hdlc_comm_func, NULL);
	}
}
