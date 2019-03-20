/*//////////////////////////////////////////////////////////////////////////////
 * Copyright: 2019 Brad Love (Next Dimension Innovations) brad@nextdimension.cc
 * SPDX-License-Identifier:	GPL-3.0-or-later
*//////////////////////////////////////////////////////////////////////////////

#define _GNU_SOURCE         /* See feature_test_macros(7) */

/* Returns a model name string for a supported vid:pid
 * or "Device Unknown"
 */
const char* device_name(const char* vend, const char* prod);

/* Query sysfs for devices currently using the em28xx driver.
 * Informational only.
 * Returns number of em28xx devices found
 */
int em28xx_device_detect();
