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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "eeprom-utils.h"
#include "linux-i2c.h"

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
			printf("           Full EEPROM Dump @ offset 0x%02X\n", hwconf_start);

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
/* Updates a single byte in an eeprom
 * returns <= 0 on error
 */
int eeprom_update(unsigned int test_flag,
		  unsigned int bus_no,
		  unsigned char w_offset,
		  unsigned char w_value)
{
	unsigned char i2c_addr = 0xa0 >> 1;
	unsigned char i2c_data[3] = { 0 };

	i2c_data[1] = w_offset;
	i2c_data[2] = w_value;

	if (test_flag) {
		printf("\nEquivalent operation for manual conversion:\n");
		printf("\n\ti2cset -y %d 0x%02x 0x%02x 0x%02x 0x%02x i\n\n", bus_no,
			i2c_addr, i2c_data[0], i2c_data[1], i2c_data[2]);
	} else {
		if (i2cset_impl(bus_no, i2c_addr, 3, i2c_data) != 0)
			return -1;
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
	unsigned int bPIDChangeAllowed = 0, bChangeAllowed = 0, bIsDefBulk = 0, bIsOrigBulk = 0;
	unsigned int bIsCurBulk = 0, bModelNotFound = 0, retval = 0;
	char sModelName[64] = { 0 };
	unsigned short vendor_ID = (tst_eeprom->vendor_ID[1] << 8) |
					tst_eeprom->vendor_ID[0];
	unsigned short product_ID = (tst_eeprom->product_ID[1] << 8) |
					tst_eeprom->product_ID[0];

	bIsDefBulk = (tst_eeprom->product_ID[1] & 0x80) != 0;
	bIsCurBulk = (tst_eeprom->BoardConfigEx & BOARD_CFG2_BULK_TS) != 0;
	bIsOrigBulk = bIsDefBulk;

//	printf("\nbIsDefBulk = %d   |   bIsOrigBulk = %d\n\n", bIsDefBulk, bIsOrigBulk);

	// Get the default Bulk/ISO flag based on known Hauppauge Model numbers
	switch (eeprom_tv->model) {
	case 203029: // soloHD DVB
	case 203039:
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
	case 200000: // 461e B8H9 - 3103b demod
		bIsDefBulk = false;
		bPIDChangeAllowed = false;
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
	} else if (vendor_ID == 0x2013) { // PCTV
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
			bChangeAllowed = true;
			break;
		case 0x0259:
		case 0x0461:
			strcpy(sModelName, "PCTV 461e (3103B)");
			bChangeAllowed = true;
			break;
		default:
			bModelNotFound = 1;
			break;
		}
	} else {
		printf("Shyzer...%s\n", sModelName);
		return -1;
	}

//	printf("bIsDefBulk = %d   |   bIsOrigBulk = %d   |   bIsCurBulk = %d\n\n", bIsDefBulk, bIsOrigBulk, bIsCurBulk);

	printf("\nFound: %s mode %s  ( %d )\n\n", bIsDefBulk ? "bulk" : "isoc",
		sModelName, eeprom_tv->model);

	if (bModelNotFound) {
		printf("VID:PID unknown, quitting...\n\n");
		return -1;
	}

	if (bIsOrigBulk) {
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

	if (bChangeAllowed) {

		if (!bIsOrigBulk && !bIsCurBulk) {
			tst_eeprom->BoardConfigEx |= BOARD_CFG2_BULK_TS;
			retval |= BULK_CONVERSION_POSSIBLE;
			bIsCurBulk = true;
		} else if (!bIsOrigBulk && bIsCurBulk) {
			printf("\nDevice %s has already been converted to bulk mode\n", sModelName);
		}

		if (bPIDChangeAllowed) {
			/* NOTE: we only change the PID value on certain known
			 * HCW Models where we can easily restore the original
			 * Bulk/ISO state.  Changing the PID has implications
			 * for processing of product returns.
			 */
			if (vendor_ID == 0x2040 && !bIsOrigBulk) { // Hauppauge
				if (bIsCurBulk) {
					tst_eeprom->product_ID[1] |= 0x80;
					retval |= PID_MODIFICATION_POSSIBLE;
				} else
					printf("\nDevice %s must be converted to bulk mode first\n", sModelName);
			} else if ((tst_eeprom->product_ID[1] & 0x80) == 0x80)
				printf("\nDevice %s PID is already modified\n", sModelName);
		}
	} else {
		printf("Device modification not allowed\n");
		return -1;
	}

	printf("\n");

	return retval;
}

