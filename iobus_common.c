#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "iobus_common.h"
#include "iobus_net.h"
#include "iobus_hdlc.h"

global_param_t glb;
int timer_cnt = 0;
int led = 0;
static void *sig_func(void *arg)
{
	int sig_no = 0;
	int ret = 0;
	for (;;)
	{
		ret = sigwait(&glb.sig_mask, &sig_no);
		if (ret != 0)
		{
			perror("Sigwait failed :");
		}
		else
		{
			switch(sig_no)
			{
				case SIGINT:
					printf("SIGINT.\n");
					break;
				case SIGQUIT:
					printf("SIGQUIT.\n");
					break;
				case SIGPIPE:
					printf("SIGPIPE.\n");
					break;
			/*	case SIGALRM:
             		glb.hdlc_ch_change_flag = false;
             		break;*/
				case SIGALRM:
//					printf("www\n");
					if (timer_cnt == 5)
					{
            	  		pthread_mutex_lock(&glb.hdlc_ch_change_mutex);
						glb.hdlc_ch_change_flag = false;
              			pthread_mutex_unlock(&glb.hdlc_ch_change_mutex);
						timer_cnt = 0;
					}
					if (glb.hdlc_ch_change_flag == true)
					{
						timer_cnt++;
					}
					if (led == 0)
					{
						led = 1;
					}
					else
					{
						led = 0;
					}
					ioctl(glb.dev_fd, IOBUS_IOC_LED_STAT, led);
					fd_set fds;
             		FD_ZERO(&fds);
            		FD_SET(glb.dev_fd, &fds);
            		if ((ret = select(glb.dev_fd+1, NULL, &fds, NULL, NULL)) < 0)
             		{
//						pthread_mutex_unlock(&glb.hdlc_mutex);
                 		return;
 					}
            		else
            		{
                 		if (FD_ISSET(glb.dev_fd, &fds))
                 		{
                     		for (;;)
                   			{
								pthread_mutex_lock(&glb.hdlc_mutex);
                         		if (write(glb.dev_fd, glb.hdlc_broadcast_buf, sizeof(glb.hdlc_broadcast_buf)) == sizeof(glb.hdlc_broadcast_buf))
                         		{
									usleep(500);
									pthread_mutex_unlock(&glb.hdlc_mutex);
									break;
                       			}
                    		 }
                 		}
            		}
					break;
				default:
					break;
			}
		}
	}
	return;
} 

bool system_init()
{
	glb.dev_fd = open(DEV_NAME, O_RDWR);
	if (glb.dev_fd == -1)
	{
		perror("Can't open device iobus: ");
		return false;
	}
	glb.net_timeout = NET_TIMEOUT;
	glb.run_stat = STOP;
	glb.hdlc_ch = HDLC_CH1;
	glb.hdlc_stat = MASTER;
	glb.hdlc_ch_change_flag = false;
	if (ioctl(glb.dev_fd, IOBUS_IOC_RUN_STAT, glb.hdlc_stat) == -1)
	{
		perror("Can't set iobus HDLC master mode :");
		return false;
	}
	if (ioctl(glb.dev_fd, IOBUS_IOC_CH_SEL, glb.hdlc_ch) == -1)
	{
		perror("Can't set iobus HDLC channel1 :");
		return false;
	}
//	signal(SIGALRM, hdlc_ch_change_sig_func);
	pthread_mutex_init(&glb.hdlc_mutex, NULL);
	pthread_mutex_init(&glb.hdlc_ch_change_mutex, NULL);
	sigemptyset(&glb.sig_mask);
//	sigaddset(&glb.sig_mask, SIGINT);
//	sigaddset(&glb.sig_mask, SIGPIPE);
//	sigaddset(&glb.sig_mask, SIGQUIT);
	sigaddset(&glb.sig_mask, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &glb.sig_mask, NULL);
	glb.hdlc_broadcast_buf[0] = 0xfe;
	glb.hdlc_broadcast_buf[1] = 0;
	glb.hdlc_broadcast_buf[2] = 0;
	glb.hdlc_broadcast_buf[3] = 0xe3;
	glb.hdlc_broadcast_buf[4] = 0x5a;
	glb.hdlc_itimerval.it_value.tv_sec = 0;
	glb.hdlc_itimerval.it_value.tv_usec = HDLC_BROADCAST_TIMEINTERVAL;
	glb.hdlc_itimerval.it_interval.tv_sec = 0;
	glb.hdlc_itimerval.it_interval.tv_usec = HDLC_BROADCAST_TIMEINTERVAL;
	setitimer(ITIMER_REAL, &glb.hdlc_itimerval, NULL);
	pthread_create(&glb.sig_tid, NULL, sig_func, NULL);
	return true;
}

void set_timeval(struct timeval *tv, long sec, int usec)
{
	tv->tv_sec = sec;
	tv->tv_usec = usec;
}
