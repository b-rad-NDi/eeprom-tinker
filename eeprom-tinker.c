/*//////////////////////////////////////////////////////////////////////////////
 * WARNING: THIS PROGRAM IS DANGEROUS AND CAN  CAUSE HARM TO YOUR SYSTEM!!!!!
 * Ok, you've been warned...it's not that dangerous as long as you use it safely
 * and wisely. Always double check you are passing the correct i2c bus.
 *
 * This source borrows source from the following kernel files:
 *  - drivers/media/usb/em28xx/em28xx-i2c.c
 *  - drivers/media/common/tveeprom.c
 *
 *
 * Copyright: 2019 Brad Love (Next Dimension Innovations) brad@nextdimension.cc
 * SPDX-License-Identifier:	GPL-3.0-or-later
*//////////////////////////////////////////////////////////////////////////////

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>


#include "device-detection.h"
#include "eeprom-utils.h"

/********************************************************************************************************/
void device_info(struct em28xx_eeprom *tst_eeprom, struct tveeprom *eeprom_tv)
{
	unsigned short vendor_ID = (tst_eeprom->vendor_ID[1]  << 8) |
					tst_eeprom->vendor_ID[0];
	unsigned short product_ID = (tst_eeprom->product_ID[1] << 8) |
					tst_eeprom->product_ID[0];

	printf("\n");
	printf("vid:pid       %04x:%04x\n", vendor_ID, product_ID);

	printf("Board Config: %02x  (%s)\n", tst_eeprom->BoardConfigEx,
		(tst_eeprom->BoardConfigEx & BOARD_CFG2_BULK_TS) ? "bulk" : "isoc");

	printf("serial:       %02x %02x %02x\n",
		(eeprom_tv->serial_number >> 16) & 0xff,
		(eeprom_tv->serial_number >> 8) & 0xff,
		eeprom_tv->serial_number && 0xff);

	printf("model:        %02x %02x %02x %02x  ( %d )\n",
		(eeprom_tv->model >> 24) & 0xff, (eeprom_tv->model >> 16) & 0xff,
		(eeprom_tv->model >> 8) & 0xff, eeprom_tv->model & 0xff, eeprom_tv->model);

	printf("revision:     %02x %02x %02x\n", (eeprom_tv->revision >> 16) & 0xff,
		(eeprom_tv->revision >> 8) & 0xff, eeprom_tv->revision & 0xff);
}

/********************************************************************************************************/
void print_help()
{
	printf("eeprom-tinker options\n");
	printf("\t-b\tset i2c bus number (dangerous! be sure this is correct!)\n");
	printf("\t-i\tprint device info\n");
	printf("\t-m\t{pid|bulk|both}  fix product id, fix bulk flag, or fix both\n");
	printf("\t-t\ttest mode, only output equivalent modification operation\n");
	printf("\t-v\tverbose\n");
	printf("\t-h\tdisplay help\n");
}

/********************************************************************************************************/
int main (int argc, char **argv)
{
	unsigned char eeprom_data[256] = { 0 };
	struct em28xx_eeprom *tst_eeprom = (struct em28xx_eeprom*)&eeprom_data[0];
	struct tveeprom eeprom_tv = { 0 };
	char i2c_bus_string[128] = { 0 };
	int hwconf_offset = 0, retval = 0, i2c_ret = 0;
	int vflag = 0, iflag = 0, c = 0, mflag = 0, bulk_flag = 0, pid_flag = 0, testonly_flag = 0;
	unsigned long i2c_bus_no = ULONG_MAX; 
	unsigned char pid_offset = 0, boardconf_offset = 0;

	printf("EEPROM Tinker\n\tWelcome to the danger zone\n");
	printf("\tBe careful, this utility can harm your system if misused!\n\n");

	if (argc == 1) {
		print_help();
		return 1;
	}

	while ((c = getopt (argc, argv, "vithm:b:")) != -1) {
		switch (c)
		{
		case 'v':
			vflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'b':
			i2c_bus_no = strtoul(optarg, NULL, 10);
			sprintf(&i2c_bus_string[0], "/dev/i2c-%lu", i2c_bus_no);
			if (access(i2c_bus_string, F_OK) != 0) {
				printf("Invalid i2c bus number %lu\n", i2c_bus_no);
				return 1;
			}
			break;
		case 'm':
			if (strncasecmp(optarg, "pid", 3) == 0)
				pid_flag = 1;
			else if (strncasecmp(optarg, "bulk", 4) == 0)
				bulk_flag = 1;
			else if (strncasecmp(optarg, "both", 4) == 0) {
				pid_flag = 1;
				bulk_flag = 1;
			} else {
				printf("Bad argument for -m\n\n");
				print_help();
				return 1;
			}
			mflag = 1;
			break;
		case 't':
			testonly_flag = 1;
			break;
		case 'h':
			print_help();
			return 1;
		case '?':
			if (optopt == 'b')
				fprintf (stderr, "Option -%c requires an argument.\n", optopt);
			else if (!isprint (optopt))
				fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
			return 1;
		default:
			abort();
		}
	}

	if (i2c_bus_no == ULONG_MAX) {
		fprintf (stderr, "Option -b is required, quitting\n");
		return 1;
	}

	hwconf_offset = eeprom_check(vflag, i2c_bus_no, &eeprom_data[0]);

	if (hwconf_offset > 0) {
		if (eeprom_decode(&eeprom_data[0], &eeprom_tv) != 0) {
			printf("Eeprom decode failure, quitting...\n");
			return -1;
		}

		eeprom_tv.byte_offset_start = hwconf_offset;
		eeprom_tv.i2c_bus_no = i2c_bus_no;

		if (iflag)
			device_info(tst_eeprom, &eeprom_tv);

		retval = eeprom_validate(0, &eeprom_data[0], &eeprom_tv);

		if (mflag && retval > 0) {
			if (bulk_flag && (retval & BULK_CONVERSION_POSSIBLE)) {
				boardconf_offset = offsetof(struct em28xx_eeprom, BoardConfigEx);
				printf("\n");
				printf("Update Board Config:%s", testonly_flag ? "\n" : " ");
				i2c_ret = eeprom_update(1, eeprom_tv.i2c_bus_no,
						eeprom_tv.byte_offset_start + boardconf_offset,
						tst_eeprom->BoardConfigEx);
				if (!testonly_flag && i2c_ret == 0)
					printf("OK\n");
				else
					return 1;
			}

			usleep(50 * 1000);

			if (pid_flag && (retval & PID_MODIFICATION_POSSIBLE)) {
				pid_offset = offsetof(struct em28xx_eeprom, product_ID[1]);
				printf("\n");
				printf("Update Product ID:%s", testonly_flag ? "\n" : " ");
				i2c_ret = eeprom_update(1, eeprom_tv.i2c_bus_no,
						eeprom_tv.byte_offset_start + pid_offset,
						tst_eeprom->product_ID[1]);

				if (!testonly_flag && i2c_ret == 0)
					printf("OK\n");
				else
					return 1;
			}

			if (testonly_flag)
				printf("\nOnly execute the above commands if you are absolutely confident\n");
		}
	} else {
		return 1;
	}

	printf("\n");

	return 0;
}

