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

#define BOARD_CFG2_BULK_VIDEO       0x01    // Video pipe in Bulk mode
#define BOARD_CFG2_BULK_AUDIO       0x02    // Audio pipe in Bulk mode
#define BOARD_CFG2_BULK_TS          0x04    // TS    pipe in Bulk mode

#define BULK_CONVERSION_POSSIBLE    0x04
#define PID_MODIFICATION_POSSIBLE   0x08

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
	unsigned int i2c_bus_no;
	unsigned int byte_offset_start;
	unsigned int has_MAC_address;
	unsigned int model;
	unsigned int revision;
	unsigned int serial_number;
	unsigned char MAC_address[6];
};


int eeprom_check(int verbose, unsigned int bus_no, unsigned char *eeprom_data);


int eeprom_update(unsigned int test_flag,
		  unsigned int bus_no,
		  unsigned char w_offset,
		  unsigned char w_value);

int eeprom_decode(unsigned char *eeprom_data, struct tveeprom *eeprom_tv);


int eeprom_validate(int do_update, unsigned char *eeprom_data, struct tveeprom *eeprom_tv);
