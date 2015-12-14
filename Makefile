OBJS = iobus_main.o iobus_common.o iobus_net.o iobus_hdlc.o iobus_crc.o
CC = arm-none-linux-gnueabi-gcc

CFLAGS = -pipe -O2 -Wall -W -D_REENTRANT 
final: $(OBJS)
	$(CC) -o iobus_app $(OBJS) $(CFLAGS) -lpthread
iobus_main.o: iobus_main.c
	$(CC) $(CFLAGS) -c iobus_main.c
iobus_common.o: iobus_common.c
	$(CC) $(CFLAGS) -c iobus_common.c
iobus_net.o: iobus_net.c
	$(CC) $(CFLAGS) -c iobus_net.c
iobus_hdlc.o: iobus_hdlc.c
	$(CC) $(CFLAGS) -c iobus_hdlc.c
iobus_crc.o: iobus_crc.c
	$(CC) $(CFLAGS) -c iobus_crc.c
	
clean:
	rm -f iobus_main.o iobus_common.o iobus_net.o iobus_hdlc.o iobus_crc.o iobus_app
