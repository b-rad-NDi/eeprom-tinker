EEPROM Tinker by Brad Love (b-rad)

=========================================

EEPROM Tinker
	Welcome to the danger zone
	Be careful, this utility can harm your system if misused!

eeprom-tinker options
	-d	device detection
	-i	print device info
	-v	verbose
	-t	test mode, only output equivalent modification operation
	-b	set i2c bus number (dangerous! be sure this is correct!)
	-m	{pid|bulk|both}  fix product id, fix bulk flag, or fix both
	-h	display help


===============================================================================
===============================================================================

To compile:

$ sudo apt-get install libi2c-dev

$ make

Note: you require libi2c-dev >= 4.0
- This means Ubuntu Xenial LTS is incompatible, Bionic+ is ok

If make reports you are missing i2c/smbus.h your library is too old.

===============================================================================
===============================================================================


To use this dangerous utility follow these steps:

	NOTE: you will require to run these commands via sudo

===============================================================================
===============================================================================

Detection of compatible devices:

$ eeprom-tinker -d

EEPROM Tinker
	Welcome to the danger zone
	Be careful, this utility can harm your system if misused!
=========================================
Found 2013:0461  -  PCTV 461e (3103B)
Root i2c bus: i2c-14   <--- use
	Child bus: i2c-15
=========================================
1 em28xx devices found
=========================================

===============================================================================
===============================================================================

Information about detected device:

$ eeprom-tinker -i -b 14

EEPROM Tinker
	Welcome to the danger zone
	Be careful, this utility can harm your system if misused!

Found tag 0x04 (serial)
Found tag 0x06 (model/revision)

vid:pid       2013:0461
Board Config: 04  (bulk)
serial:       d6 42 01
model:        00 03 0d 40  ( 200000 )
revision:     89 8a 19

Found: isoc mode PCTV 461e (3103B)  ( 200000 )
Conversion to bulk mode possible: yes
Convert device to new PID? possible
Change allowed


===============================================================================
===============================================================================

Conversion of a compatible device:

$ eeprom-tinker -t -b 14 -m bulk

EEPROM Tinker
	Welcome to the danger zone
	Be careful, this utility can harm your system if misused!

Found tag 0x04 (serial)
Found tag 0x06 (model/revision)

Found: isoc mode PCTV 461e (3103B)  ( 200000 )
Conversion to bulk mode possible: yes
Convert device to new PID? possible
Change allowed


Update Board Config:
Equivalent operation:
i2cset -y 14 0x50 0x00 0xa7 0x04 i


===============================================================================
===============================================================================

FINALLY

This utility *can* do the modification for you, but this is currently disabled
in the source code. Instead it prints out the explicit operation required to
do the conversion yourself.

For the device being operated on in this README, the following command will
convert the 461e from ISOC to bulk mode:


$ sudo i2cset -y 14 0x50 0x00 0xa7 0x04 i


===============================================================================
===============================================================================

Once the i2cset operation has been executed, wait a few seconds
(for your sanity), then you can unplug and reconnect the device.

You can then dump device info, with this utility or dmesg to verify
the device is now in bulk mode:


$ eeprom-tinker -i -b 14

EEPROM Tinker
	Welcome to the danger zone
	Be careful, this utility can harm your system if misused!

Found tag 0x04 (serial)
Found tag 0x06 (model/revision)

vid:pid       2013:0461
Board Config: 04  (bulk)
serial:       d6 42 01
model:        00 03 0d 40  ( 200000 )
revision:     89 8a 19

Found: isoc mode PCTV 461e (3103B)  ( 200000 )
Conversion to bulk mode possible: yes
Convert device to new PID? possible
Change allowed


=========================================

$ dmesg | egrep -i '461|bulk'

[70053.597329] usb 3-1: New USB device found, idVendor=2013, idProduct=0461, bcdDevice= 1.00
[70053.597337] usb 3-1: Product: PCTV 461
[70053.598730] em28xx 3-1:1.0: New device PCTV PCTV 461 @ 480 Mbps (2013:0461, interface 0, class 0)
[70053.598734] em28xx 3-1:1.0: DVB interface 0 found: bulk
[70054.108045] em28xx 3-1:1.0: Identified as PCTV DVB-S2 Stick (461e v2) (card=104)
[70054.108051] em28xx 3-1:1.0: dvb set to bulk mode.


Did it work? It better have :-D


===============================================================================
===============================================================================


USE AT YOUR OWN RISK!!!!
USE AT YOUR OWN RISK!!!!
USE AT YOUR OWN RISK!!!!
USE AT YOUR OWN RISK!!!!
USE AT YOUR OWN RISK!!!!


===============================================================================
===============================================================================

