/*//////////////////////////////////////////////////////////////////////////////
 * Copyright: 2019 Brad Love (Next Dimension Innovations) brad@nextdimension.cc
 * SPDX-License-Identifier:	GPL-3.0-or-later
*//////////////////////////////////////////////////////////////////////////////

#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int is_i2c_bus(const struct dirent* entry)
{
        return (strncasecmp(entry->d_name, "i2c-", 4) == 0) ? 1 : 0;
}

int file_get_contents(const char *filename, char **result)
{
	unsigned int size = 0, rsize = 0;
	FILE *f = fopen(filename, "rb");
	if (f == NULL) {
		*result = NULL;
		return -1; // -1 means file opening fail
	}
	size = 64;
	if ((*result = (char *)malloc(size+1)) == NULL) exit(254);

	if ((rsize = fread(*result, sizeof(char), size, f)) == 0) {
		fclose(f);
		free(*result);
		return -2; // -2 means file reading fail
	}

	fclose(f);
	if (rsize >= 1)
		(*result)[rsize-1] = 0;
	else
		(*result)[rsize] = 0;

	return rsize;
}

int find_i2c_devs(const char* full_path)
{
	struct dirent **namelist;
	int n, i;

	n = scandir(full_path, &namelist, is_i2c_bus, versionsort);
	if (n < 0)
		perror("scandir");
	else {
		for (i = 0; i < n; i++) {
			if (i == 0)
				printf("Root i2c bus: %s   <--- use \n", namelist[i]->d_name);
			else
				printf("\tChild bus: %s\n", namelist[i]->d_name);

			free(namelist[i]);
		}
		free(namelist);
	}

	return -1;
}

const char* device_name(const char* vend, const char* prod)
{
	int vendor_ID = 0, product_ID = 0;
	char *end_ptr = NULL;

	vendor_ID = strtoul(vend, &end_ptr, 16);
	if (*end_ptr != '\0')
		printf("Shouldn't be here...\n");

	product_ID = strtoul(prod, &end_ptr, 16);
	if (*end_ptr != '\0')
		printf("Shouldn't be here...\n");

	if (vendor_ID == 0x2040) { // Hauppauge
		switch (product_ID & 0x7fff) {
		case 0x0264:
			return "SoloHD DVB";
			break;
		case 0x0265:
			return "DualHD DVB";
			break;
		case 0x0266:
			return "SoloHD ISDBT/DVBT/DVBC";
			break;
		case 0x026A:
			return "SoloHD ISDBT/DVBT/DVBC (OEM)";
			break;
		case 0x026d:
			return "DualHD ATSC";
			break;
		case 0x026e:
			return "DualHD ATSC (DTV)";
			break;
		case 0x0269:
			return "DualHD DVB (DN)";
			break;
		case 0x026f:
			return "DualHD ATSC (DN)";
			break;
		case 0x0270:
			return "DualHD ATSC (AZ)";
			break;
		case 0x0271:
			return "DualHD ATSC (OEM)";
			break;
		case 0x0278:
			return "DualHD DVB (AZ)";
			break;
		default:
			return "Device Unknown (Hauppauge)";
			break;
		}	
	} else if (vendor_ID == 0x2013) { // PCTV
		switch (product_ID & 0x7fff) {
		case 0x0265:
			return "PCTV 2920e (204009)";
			break;
		case 0x025f:
			return "PCTV 292e";
			break;
		case 0x0264:
			return "PCTV 292e SE";
			break;
		case 0x0258:
			return "PCTV 461e";
			break;
		case 0x0259:
		case 0x0461:
			return "PCTV 461e (3103B)";
			break;
		default:
			return "Device Unknown (PCTV)";
			break;
		}
	}

	return "Device Unknown";
}

int em28xx_device_detect()
{
	struct dirent **namelist;
	char full_dev_path[256] = { 0 };
	char vend_path[256] = { 0 };
	char prod_path[256] = { 0 };
	char *vend_id = NULL, *prod_id = NULL;
	const char *dev_name = NULL;
	struct stat buf = { 0 };
	int n, x, retval = 0;

	n = scandir("/sys/bus/usb/drivers/em28xx/", &namelist, NULL, alphasort);
	chdir("/sys/bus/usb/drivers/em28xx/");

	if (n < 0)
		perror("scandir");
	else {
		while (n--) {
			x = lstat (namelist[n]->d_name, &buf);
			if ((x == 0) && S_ISLNK(buf.st_mode) &&
			   (realpath(namelist[n]->d_name, &full_dev_path[0]) != NULL)) {
				if (!isdigit(namelist[n]->d_name[0])) {
					free(namelist[n]);
					continue;
				}

				snprintf(&vend_path[0], 256, "%s/../idVendor", full_dev_path);
				snprintf(&prod_path[0], 256, "%s/../idProduct", full_dev_path);

				if ((file_get_contents(vend_path, &vend_id) <= 0) ||
				   (file_get_contents(prod_path, &prod_id) <= 0)) {
					free(namelist[n]);
					continue;
				}

				printf("=========================================\n");
				dev_name = device_name(vend_id, prod_id);
				printf("Found %s:%s  -  %s\n", vend_id, prod_id, dev_name);
//				printf("Device: %s\n", full_dev_path);

				if (strncmp(dev_name, "Device Unknown", 14) != 0)
					find_i2c_devs(full_dev_path);

				retval++;
				free(vend_id);
				free(prod_id);
			}
//			printf("%s\n", namelist[n]->d_name);
			free(namelist[n]);
		}
		free(namelist);
	}

	return retval;
}
