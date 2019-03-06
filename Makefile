CC=$(CROSS_COMPILE)gcc
CFLAGS=$(CFLAGS)
LDFLAGS=$(LDFLAGS)

eeprom-tinker : eeprom-tinker.o linux-i2c.o
	$(CC) -o eeprom-tinker eeprom-tinker.o linux-i2c.o -li2c

linux-i2c.o : linux-i2c.c
	$(CC) -c linux-i2c.c

eeprom-tinker.o : eeprom-tinker.c linux-i2c.c
	$(CC) -c eeprom-tinker.c

clean :
	rm -f eeprom-tinker eeprom-tinker.o linux-i2c.o
