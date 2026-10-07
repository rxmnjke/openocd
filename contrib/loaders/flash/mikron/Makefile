BIN2C = ../../../../src/helper/bin2char.sh

CROSS_COMPILE ?= riscv64-unknown-elf-

CC=$(CROSS_COMPILE)gcc
OBJCOPY=$(CROSS_COMPILE)objcopy
OBJDUMP=$(CROSS_COMPILE)objdump

CFLAGS = -march=rv32imc_zicsr -mabi=ilp32 -static -nostartfiles -nostdlib -Os -g -fPIC -Wl,-T,ram.ld

all: mik32_init_spifi_read.inc mik32_init_spifi_write.inc mik32_spifi_chip_erase.inc mik32_spifi_sector_erase.inc mik32_spifi_write_sector.inc mik32_eeprom_sector_erase.inc mik32_eeprom_write_sector.inc
     

.PHONY: clean

%.elf: %.c
	$(CC) $(CFLAGS) $< crt0.S -o $@

%.S: %.c
	$(CC) $(CFLAGS) -S $< -o $@

%.lst: %.elf
	$(OBJDUMP) -S $< > $@

%.bin: %.elf
	$(OBJCOPY) -Obinary $< $@

%.inc: %.bin
	$(BIN2C) < $< > $@

clean:
	-rm -f *.elf *.lst *.bin *.inc
