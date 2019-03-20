CC=$(CROSS_COMPILE)gcc
CFLAGS=$(CFLAGS)
LDFLAGS=$(LDFLAGS)

eeprom-tinker : eeprom-tinker.o linux-i2c.o eeprom-utils.o device-detection.o
	$(CC) -g -o eeprom-tinker eeprom-tinker.o linux-i2c.o eeprom-utils.o device-detection.o -li2c

eeprom-utils.o : eeprom-utils.c linux-i2c.c
	$(CC) -g -Wall -c eeprom-utils.c

linux-i2c.o : linux-i2c.c
	$(CC) -g -Wall -c linux-i2c.c

device-detection.o : device-detection.c
	$(CC) -g -Wall -c device-detection.c

eeprom-tinker.o : eeprom-tinker.c linux-i2c.c eeprom-utils.c device-detection.c
	$(CC) -g -Wall -c eeprom-tinker.c

clean :
	rm -f eeprom-tinker eeprom-tinker.o linux-i2c.o eeprom-utils.o device-detection.o
