/*//////////////////////////////////////////////////////////////////////////////
 * linux-i2c is an "easy to use" wrapper around libi2c from i2c-tools
 * It pretty much copies the source from i2cget and i2cset offering
 * a programmatic approach to utilizing those utilities.
 *
 *
 * Copyright: 2018 Brad Love (Next Dimension Innovations) brad@nextdimension.cc
 * SPDX-License-Identifier:	GPL-3.0-or-later
*//////////////////////////////////////////////////////////////////////////////

#ifndef _LINUX_I2C_
#define _LINUX_I2C_

#ifdef __cplusplus
extern "C" {
#endif

int i2cget_impl(unsigned char        bus,
		unsigned char        device_adr,
		unsigned int         read_size,
		unsigned char       *p_read_buf);

int i2cset_impl(unsigned char        bus,
		unsigned char        device_adr,
		unsigned int         write_size,
		const unsigned char *p_write_buf);

#ifdef __cplusplus
}
#endif

#endif

