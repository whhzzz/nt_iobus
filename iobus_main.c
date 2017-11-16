#include <stdio.h>
#include <stdlib.h>


#include "iobus_common.h"
#include "iobus_net.h"
#include "iobus_hdlc.h"



int main(int argc, char *argv[])
{
	bool ret = 0;
	ret = system_init();
	if (ret != true)
	{
		printf("system initial error\n");
		exit(1);
	}
	ret = create_server_socket();
	if (ret != true)
	{
		printf("create server socket failed\n");
		exit(1);
	}
	net_data_recv_loop();
	printf("exception return from main loop\n");
	return 0;
}

