#ifndef _STREAM_GEN_APP_H_
#define _STREAM_GEN_APP_H_

#define CONFIG_REG	0x80
#define CTRL_SET_REG	0x81
#define CTRL_CLR_REG	0x82
#define SIZE_REG	0x83
#define STATUS_REG	0x84

int rd_reg(int fd, unsigned int reg, unsigned int *ptr);
int wr_reg(int fd, unsigned int reg, unsigned int *ptr);

#endif /* _STREAM_GEN_APP_H_ */
