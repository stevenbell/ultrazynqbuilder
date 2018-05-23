#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "stream-gen.h"

int main(int argc, char **argv)
{
    int status, fd;
    unsigned int val;

	fd = open("/dev/streamgen0", O_RDWR);
	if(fd < 0) {
		printf("[OUT] failed to open device driver\n");
		return -1;
	}
	printf("[OUT] successfully opened device driver\n");

	printf("[OUT] ============== device test start ==================\n");
	
	// write configuration
	val = 0; // Mode = 0 (Run/Stop), Interrupts Disabled, TLAST signal enabled
	status = wr_reg(fd, CONFIG_REG, &val);
	if(status < 0) {
		printf("[OUT] failed to write to CONFIG_REG\n");
		goto cleanup;
	}

	// write SIZE reg
	val = 1024; // 1024 16-bit values (corresponding to a line of 1024 pixels with depth of 2 bytes)
	status = wr_reg(fd, SIZE_REG, &val);
	if(status < 0) {
		printf("[OUT] failed to write to STATUS_REG\n");
		goto cleanup;
	}

	// confirm size
	status = rd_reg(fd, SIZE_REG, &val);
	if(status < 0 || val != 1024) {
		printf("[OUT] SIZE_REG value mismatch\n");
		goto cleanup;
	}

	// run
	val = 1;
	status = wr_reg(fd, CTRL_SET_REG, &val);
	if(status < 0) {
		printf("[OUT] failed to write to CTRL_SET_REG\n");
		goto cleanup;
	}

	// wait until stop
	while(val & 0x01) {
		status = rd_reg(fd, CONFIG_REG, &val);
	}
	printf("[OUT] output complete\n");

	printf("[OUT] ============== device test stop ==================\n");

	cleanup: close(fd);
    return 0;
}
