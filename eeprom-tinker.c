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
 * Copyright: 2018 Brad Love (Next Dimension Innovations) brad@nextdimension.cc
 * SPDX-License-Identifier:	GPL-3.0-or-later
*//////////////////////////////////////////////////////////////////////////////

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "linux-i2c.h"

#define BOARD_CFG2_BULK_VIDEO       0x01    // Video pipe in Bulk mode
#define BOARD_CFG2_BULK_AUDIO       0x02    // Audio pipe in Bulk mode
#define BOARD_CFG2_BULK_TS          0x04    // TS    pipe in Bulk mode

struct __attribute__((__packed__)) em28xx_eeprom {
	unsigned char id[4];		/* 1a eb 67 95 */
	unsigned char vendor_ID[2];
	unsigned char product_ID[2];
	unsigned char chip_conf[2];
	unsigned char board_conf[2]; /* tuner_config | board config */

	unsigned char string1[2]; /* offset | length */
	unsigned char string2[2]; /* offset | length */
	unsigned char string3[2]; /* offset | length */
	unsigned char string_idx_table;
	unsigned char driver_length;

	unsigned char eeprom_filler[47]; /* non-public info */

	unsigned char BoardConfigEx; /* board config extensions */
};

struct tveeprom {
	unsigned int byte_offset_start;
        unsigned int has_MAC_address;
        unsigned int model;
        unsigned int revision;
        unsigned int serial_number;
        unsigned char MAC_address[6];
};

/********************************************************************************************************/
/* returns <= 0 on error
 * return byte offset of the start of the tveeprom
 */
int eeprom_check(int verbose, unsigned int bus_no, unsigned char *eeprom_data)
{
	unsigned char i2c_addr = 0xa0 >> 1;
	unsigned char i2c_data[3] = { 0 };
	int i = 0, j = 0, mc_start = 0, hwconf_start = 0;

	if (i2cset_impl(bus_no, i2c_addr, 2, i2c_data) != 0) {
		printf("Error, probably require sudo...\n");
		return -1;
	}

	for (i = 0; i < 256; i++) {
		if (i2cget_impl(bus_no, i2c_addr, 1, &eeprom_data[i]) != 0)
			return -1;
	}

	if (eeprom_data[0] == 0x26 && eeprom_data[3] == 0x00) {
		mc_start = (eeprom_data[1] << 8) + 4; /* should be 0x04 */
		hwconf_start = mc_start + eeprom_data[mc_start + 46] +
				(eeprom_data[mc_start + 47] << 8); /* should total 0x64 */

		if (eeprom_data[hwconf_start + 0] != 0x1a ||
		    eeprom_data[hwconf_start + 1] != 0xeb ||
		    eeprom_data[hwconf_start + 2] != 0x67 ||
		    eeprom_data[hwconf_start + 3] != 0x95) {
			printf("Error: Empia eeprom signature missing @ 0x%02x!\n", hwconf_start);
			return -1;
		}

		i2c_data[0] = 0x00;
		i2c_data[1] = hwconf_start;

		if (i2cset_impl(bus_no, i2c_addr, 2, i2c_data) != 0)
			return -1;

		for (i = 0; i < 256; i++) {
			if (i2cget_impl(bus_no, i2c_addr, 1, &eeprom_data[i]) != 0)
				return -1;
		}

		if (verbose != 0) {
			printf("-----------------------------------------------------\n");
			printf("                  Full EEPROM Dump\n");

			for (i = 0; i < 16; i++) {
				printf("%03x0: ", i);
				for (j = 0; j < 16; j++)
					printf("%02x ", eeprom_data[i * 16 + j]);

				printf("\n");
			}
			printf("-----------------------------------------------------\n");
			printf("\n");
		}

		return hwconf_start;
	}

	return 0;
}

/********************************************************************************************************/
/* Peruses eeprom looking for "tags" containing serial/MAC/model/revision
 */
int eeprom_decode(unsigned char *eeprom_data, struct tveeprom *eeprom_tv)
{
	int start = 0xa0, len = 256, done = 0, i = 0;
	unsigned char tag;

	for (i = start; !done && i < 256; i += len) {
		if (eeprom_data[i] == 0x84) {
			len = eeprom_data[i + 1] + (eeprom_data[i + 2] << 8);
			i += 3;
		} else if ((eeprom_data[i] & 0xf0) == 0x70) {
			if (eeprom_data[i] & 0x08) {
				/* verify checksum! */
				done = 1;
				break;
			}
			len = eeprom_data[i] & 0x07;
			++i;
		} else {
			printf("Encountered bad packet header [%02x]. Corrupt or not a Hauppauge eeprom.\n",
				eeprom_data[i]);
			return -1;
		}

		/* process by tag */
		tag = eeprom_data[i];

		switch (tag) {
		case 0x01:
			/* tag: 'SerialID' */
			printf("Found tag 0x%02x (serial)\n", tag);
			eeprom_tv->serial_number =
				eeprom_data[i+6] +
				(eeprom_data[i+7] << 8) +
				(eeprom_data[i+8] << 16);
			break;
		case 0x04:
			/* tag 'SerialID2' */
			printf("Found tag 0x%02x (serial)\n", tag);
			eeprom_tv->serial_number =
				eeprom_data[i+5] +
				(eeprom_data[i+6] << 8) +
				(eeprom_data[i+7] << 16)+
				(eeprom_data[i+8] << 24);

			if (eeprom_data[i + 8] == 0xf0) {
				printf("Found MAC address found\n");
				eeprom_tv->MAC_address[0] = 0x00;
				eeprom_tv->MAC_address[1] = 0x0D;
				eeprom_tv->MAC_address[2] = 0xFE;
				eeprom_tv->MAC_address[3] = eeprom_data[i + 7];
				eeprom_tv->MAC_address[4] = eeprom_data[i + 6];
				eeprom_tv->MAC_address[5] = eeprom_data[i + 5];
				eeprom_tv->has_MAC_address = 1;
			}
			break;

		case 0x06:
			/* tag 'ModelRev' */
			printf("Found tag 0x%02x (model/revision)\n", tag);
			eeprom_tv->model =
				eeprom_data[i + 1] +
				(eeprom_data[i + 2] << 8) +
				(eeprom_data[i + 3] << 16) +
				(eeprom_data[i + 4] << 24);
			eeprom_tv->revision =
				eeprom_data[i + 5] +
				(eeprom_data[i + 6] << 8) +
				(eeprom_data[i + 7] << 16);
			break;
		default:
			break;
		}
	}

	return 0;
}

/********************************************************************************************************/
/* analyzes values found in eeprom and determines if modification can take place.
 */
int eeprom_validate(int do_update, unsigned char *eeprom_data, struct tveeprom *eeprom_tv)
{
	struct em28xx_eeprom *tst_eeprom = (struct em28xx_eeprom*)&eeprom_data[0];
	unsigned int bPIDChangeAllowed = 0, bChangeAllowed = 0, bIsDefBulk = 0;
	unsigned int bIsCurBulk = 0, bModelNotFound = 0;
	char sModelName[64] = { 0 };
	unsigned short vendor_ID = (tst_eeprom->vendor_ID[1] << 8) |
					tst_eeprom->vendor_ID[0];
	unsigned short product_ID = (tst_eeprom->product_ID[1] << 8) |
					tst_eeprom->product_ID[0];

	bIsDefBulk = (tst_eeprom->product_ID[1] & 0x80) != 0;
	bIsCurBulk = (tst_eeprom->BoardConfigEx & BOARD_CFG2_BULK_TS) != 0;

	// Get the default Bulk/ISO flag based on known Hauppauge Model numbers
	switch (eeprom_tv->model) {
	case 203029: // soloHD DVB
	case 203039: // soloHD DVB
		bIsDefBulk = false;
		bPIDChangeAllowed = true;
		break;
	case 203129:
	case 203139:
		bIsDefBulk = true;
		bPIDChangeAllowed = true;
		break;
	case 204101: //dualHD ATSC
		bIsDefBulk = false;
		bPIDChangeAllowed = true;
		break;
	case 204201:
		bIsDefBulk = true;
		bPIDChangeAllowed = true;
		break;
	case 204109: //dualHD DVB
		bIsDefBulk = false;
		bPIDChangeAllowed = true;
		break;
	case 204209:
		bIsDefBulk = true;
		bPIDChangeAllowed = true;
		break;
	}

	strcpy(sModelName, "Unknown Device, contact support");

	if (vendor_ID == 0x2040) { // Hauppauge
		switch (product_ID & 0x7fff) {
		case 0x0264:
			strcpy(sModelName, "SoloHD DVB");
			bChangeAllowed = true;
			break;
		case 0x0265:
			strcpy(sModelName, "DualHD DVB");
			bChangeAllowed = true;
			break;
		case 0x0266:
			strcpy(sModelName, "SoloHD ISDBT/DVBT/DVBC");
			bChangeAllowed = true;
			break;
		case 0x026A:
			strcpy(sModelName, "SoloHD ISDBT/DVBT/DVBC (OEM)");
			bChangeAllowed = false;
			break;
		case 0x026d:
			strcpy(sModelName, "DualHD ATSC");
			bChangeAllowed = true;
			break;
		case 0x026e:
			strcpy(sModelName, "DualHD ATSC (DTV)");
			bChangeAllowed = true;
			break;
		case 0x0269:
			strcpy(sModelName, "DualHD DVB (DN)");
			bChangeAllowed = false;
			break;
		case 0x026f:
			strcpy(sModelName, "DualHD ATSC (DN)");
			bChangeAllowed = false;
			break;
		case 0x0270:
			strcpy(sModelName, "DualHD ATSC (AZ)");
			bChangeAllowed = false;
			break;
		case 0x0271:
			strcpy(sModelName, "DualHD ATSC (OEM)");
			bChangeAllowed = false;
			break;
		case 0x0278:
			strcpy(sModelName, "DualHD DVB (AZ)");
			bChangeAllowed = false;
			break;
		default:
			bModelNotFound = 1;
			break;
		}	
	} else if (vendor_ID == 02013) { // PCTV
		switch (product_ID & 0x7fff) {
		case 0x0265:
			strcpy(sModelName, "PCTV 2920e (204009)");
			bChangeAllowed = true;
			break;
		case 0x025f:
			strcpy(sModelName, "PCTV 292e");
			bChangeAllowed = true;
			break;
		case 0x0264:
			strcpy(sModelName, "PCTV 292e SE");
			bChangeAllowed = true;
			break;
		case 0x0258:
			strcpy(sModelName, "PCTV 461e");
			bChangeAllowed = false;
			break;
		case 0x0259:
			strcpy(sModelName, "PCTV 461e (3103B)");
			bChangeAllowed = true;
			break;
		default:
			bModelNotFound = 1;
			break;
		}
	} else {
		printf("Shyzer...%s\n", sModelName);
		return 1;
	}

	printf("\nFound: %s mode %s  ( %d )\n", bIsDefBulk ? "bulk" : "isoc",
		sModelName, eeprom_tv->model);

	if (bModelNotFound) {
		printf("VID:PID unknown, quitting...\n\n");
		return 1;
	}

	if (bIsCurBulk) {
		printf("Device is already in bulk mode\n");
	} else {
		printf("Conversion to bulk mode possible: %s\n",
			(!bIsDefBulk && bChangeAllowed) ? "yes" : "no");
	}

	printf("Convert device to new PID? %s\n",
		(!bIsDefBulk && bPIDChangeAllowed)  ? "possible" : "OK as is");

	if (!bChangeAllowed ) {
		 bPIDChangeAllowed = false;
	}

	if (bIsCurBulk) {
		tst_eeprom->BoardConfigEx |= BOARD_CFG2_BULK_TS;
	} else {
		tst_eeprom->BoardConfigEx &= ~BOARD_CFG2_BULK_TS;
	}

	if (bPIDChangeAllowed) {
		/* NOTE: we only change the PID value on certain known
		 * HCW Models where we can easily restore the original
		 * Bulk/ISO state.  Changing the PID has implications
		 * for processing of product returns.
		 */
		if (vendor_ID == 0x2040) { // Hauppauge
			tst_eeprom->product_ID[1] = (tst_eeprom->product_ID[1] & 0x7F);
			if (bIsCurBulk) {
				tst_eeprom->product_ID[1] |= 0x80;
			}
		}
	} else {
		printf("Device modification not allowed\n");
		return 1;
	}

	return 0;
}

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
	int hwconf_offset = 0;
	int vflag = 0, iflag = 0, c = 0;
	unsigned long i2c_bus_no = ULONG_MAX; 

	printf("EEPROM Tinker\n\tWelcome to the danger zone\n");
	printf("\tBe careful, this utility can harm your system if misused!\n\n");

	if (argc == 1) {
		print_help();
		return 1;
	}

	while ((c = getopt (argc, argv, "vihb:")) != -1) {
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

		if (iflag)
			device_info(tst_eeprom, &eeprom_tv);

		eeprom_validate(0, &eeprom_data[0], &eeprom_tv);
	} else {
		return 1;
	}

	printf("\n");

	return 0;
}

