#include <linux/ioctl.h>
#include <sys/ioctl.h>

#include "stream-gen.h"
#include "ioctl_cmds.h"

int rd_reg(int fd, unsigned int reg, unsigned int *ptr)
{
	unsigned int cmd = 0;
	switch(reg) {
		case CONFIG_REG: cmd = RD_CONFIG; break;
		case SIZE_REG: cmd = RD_SIZE; break;
		case STATUS_REG: cmd = RD_STATUS; break;
		default: cmd = 0; break;
	}
	if(!cmd) return -1;
	return ioctl(fd, cmd, ptr);
}

int wr_reg(int fd, unsigned int reg, unsigned int *ptr)
{
	unsigned int cmd = 0;
	switch(reg) {
		case CONFIG_REG: cmd = WR_CONFIG; break;
		case SIZE_REG: cmd = WR_SIZE; break;
		case CTRL_SET_REG: cmd = WR_CTRL_SET; break;
		case CTRL_CLR_REG: cmd = WR_CTRL_CLR; break;
		default: cmd = 0; break;
	}
	if(!cmd) return -1;
	return ioctl(fd, cmd, ptr);
}

