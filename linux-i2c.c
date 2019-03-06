/*//////////////////////////////////////////////////////////////////////////////
 * linux-i2c is an "easy to use" wrapper around libi2c from i2c-tools
 * It pretty much copies the source from i2cget and i2cset offering
 * a programmatic approach to interfacing those utilities operations.
 *
 *
 * Copyright: 2018 Brad Love (Next Dimension Innovations) brad@nextdimension.cc
 * SPDX-License-Identifier:	GPL-3.0-or-later
*//////////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <fcntl.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "linux-i2c.h"

#if __linux__

#ifdef __cplusplus
extern "C" {
#endif

static int set_slave_addr(int file, int address, int force)
{
	/* With force, let the user read from/write to the registers
	   even when a driver is also running */
	if (ioctl(file, force ? I2C_SLAVE_FORCE : I2C_SLAVE, address) < 0) {
		printf("Error: Could not set address to 0x%02x: %s\n",
			address, strerror(errno));
		return -errno;
	}

	return 0;
}

static int open_i2c_dev(int i2cbus, char *filename, size_t size, int quiet)
{
	int file = -1;

	if (file < 0) { // && (errno == ENOENT || errno == ENOTDIR)) {
		sprintf(filename, "/dev/i2c-%d", i2cbus);
		file = open(filename, O_RDWR);
	}

	if (file < 0 && !quiet) {
		printf("Error opening file "
				"`%s': %s\n", filename, strerror(errno));
	}

	return file;
}

static int check_funcs(int file, int size, int pec)
{
#define MISSING_FUNC_FMT "Missing i2c function: %s"

	unsigned long funcs;

	/* check adapter functionality */
	if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
		printf("Error with adapter function list: %s\n", strerror(errno));
		return -1;
	}

	switch (size) {
	case I2C_SMBUS_BYTE:
		if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE)) {
			printf(MISSING_FUNC_FMT, "SMBus send byte");
			return -1;
		}
		break;

	case I2C_SMBUS_BYTE_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
			printf(MISSING_FUNC_FMT, "SMBus write byte");
			return -1;
		}
		break;

	case I2C_SMBUS_WORD_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_WRITE_WORD_DATA)) {
			printf(MISSING_FUNC_FMT, "SMBus write word");
			return -1;
		}
		break;

	case I2C_SMBUS_BLOCK_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_WRITE_BLOCK_DATA)) {
			printf(MISSING_FUNC_FMT, "SMBus block write");
			return -1;
		}
		break;
	case I2C_SMBUS_I2C_BLOCK_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)) {
			printf(MISSING_FUNC_FMT, "I2C block write");
			return -1;
		}
		break;
	}

	return 0;
}

int i2cset_impl(unsigned char        bus,
		unsigned char        device_adr,
		unsigned int         write_size,
		const unsigned char *p_write_buf)
{
	int res = 0, i2cbus, address, size, file;
	int value = 0, daddress;
	char filename[20];
//	int len = 0;

	i2cbus = bus;
	address = device_adr;
	daddress = p_write_buf[0];
	switch (write_size - 1) {
	case 0:
		size = I2C_SMBUS_BYTE;
		break;
	case 1:
		size = I2C_SMBUS_BYTE_DATA;
		value = p_write_buf[1];
		break;
	default:
		size = I2C_SMBUS_I2C_BLOCK_DATA;
//		len = write_size - 1;
		value = p_write_buf[1];
		break;
	}

	file = open_i2c_dev(i2cbus, filename, sizeof(filename), 0);

	if (file < 0 || check_funcs(file, size, 0) ||
	    set_slave_addr(file, address, 0)) {
		printf("SHIT HIT THE FAN!?!\n");
		if (file >= 0)
			close(file);
		return -1;
	}

	switch (size) {
	case I2C_SMBUS_BYTE_DATA:
//		printf("%s() WRITING I2C_SMBUS_BYTE_DATA @%02x | %02X : %02X\n", __func__, device_adr, daddress, value);
		res = i2c_smbus_write_byte_data(file, daddress, value);
		break;
	case I2C_SMBUS_I2C_BLOCK_DATA:
#if 0
//		printf("%s() WRITING I2C_SMBUS_I2C_BLOCK_DATA\n", __func__);
		res = i2c_smbus_write_i2c_block_data(file, daddress, len, p_write_buf + 1);
#endif
		break;
	default:
		printf("Invalid i2c transaction\n");
		res = -1;
		break;
	}
	if (res < 0) {
		printf("i2c Write failed\n");
		close(file);
		return 1;
	}

	close(file);
	return 0;
}

int i2cget_impl(unsigned char  bus,
		unsigned char  device_adr,
		unsigned int   read_size,
		unsigned char *p_read_buf)
{
	int res = 0, i2cbus, address, size, file;
	char filename[20];

	i2cbus = bus;
	address = device_adr;

	switch (read_size) {
	case 1:
//		printf("%s() I2C_SMBUS_BYTE\n", __func__);
		size = I2C_SMBUS_BYTE;
		break;
	case 's': size = I2C_SMBUS_BLOCK_DATA; break;
	case 'i': size = I2C_SMBUS_I2C_BLOCK_DATA; break;
	default:
//		printf("%s() I2C_SMBUS_I2C_BLOCK_DATA\n", __func__);
		size = I2C_SMBUS_I2C_BLOCK_DATA;
		break;
	}

	file = open_i2c_dev(i2cbus, filename, sizeof(filename), 0);

	if (file < 0 || check_funcs(file, size, 0) ||
	    set_slave_addr(file, address, 0))
	{
		printf("SHIT HIT THE FAN!?!\n");
		if (file >= 0)
			close(file);
		return -1;
	}

	switch (size) {
	case I2C_SMBUS_BYTE:
		res = i2c_smbus_read_byte(file);
		p_read_buf[0] = res & 0xFF;
		break;
	case I2C_SMBUS_I2C_BLOCK_DATA: /* I2C_SMBUS_I2C_BLOCK_DATA */
#if 0
		for (res = 0; res < read_size; res += i) {
			if (res + 32 > read_size)
			{
				rsize = read_size - res;
			}
			i = i2c_smbus_read_i2c_block_data(file,
					daddress + res, rsize, p_read_buf + res);
			if (i <= 0) {
				res = i;
				break;
			}
		}
#endif
		break;
	default:
		printf("Invalid i2c transaction\n");
		res = -1;
		break;
	}
	close(file);

	if (res < 0)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

#ifdef __cplusplus
}
#endif

#endif



