#ifndef _IOBUS_CRC_H_
#define _IOBUS_CRC_H_

extern unsigned short crc_ta[256];
extern unsigned short crc_calc(unsigned char *data,unsigned short count);

#endif

