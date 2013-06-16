include starlet.mk

# enabled usbgecko support
CFLAGS += -DCAN_HAZ_USBGECKO
# disables debug spew over usbgecko
#CFLAGS += -DNDEBUG
# enables LFCR linefeeds for usbgecko output, useful for some terminal apps
#CFLAGS += -DGECKO_LFCR
# uses the 'safe' version of the usbgecko receive and send functions
#CFLAGS += -DGECKO_SAFE

ASFLAGS += -D_LANGUAGE_ASSEMBLY
CFLAGS += -DCAN_HAZ_IRQ -DCAN_HAZ_IPC
LDSCRIPT = mini.ld
LIBS = -lgcc -lz

ELFLOADER = target/elfloader.bin
MAKEBIN = $(CURDIR)/makebin.py

TARGET = target/armboot-sym.elf
TARGET_STRIPPED = target/armboot.elf
TARGET_BIN = target/armboot.bin
OBJS = start.o main.o ipc.o vsprintf.o string.o gecko.o memory.o memory_asm.o \
	utils_asm.o utils.o ff.o diskio.o sdhc.o powerpc_elf.o powerpc.o panic.o \
	irq.o irq_asm.o exception.o exception_asm.o seeprom.o crypto.o nand.o \
	boot2.o ldhack.o sdmmc.o

include common.mk

all: $(TARGET_BIN)

main.o: main.c

$(TARGET_STRIPPED): $(TARGET)
	@echo  "STRIP	$@"
	@$(STRIP) $< -o $@

$(TARGET_BIN): $(TARGET_STRIPPED) $(ELFLOADER)
	echo  "MAKEBIN	$@"
	@$(MAKEBIN) $(ELFLOADER) $< $@

upload: $(TARGET_BIN)
	@$(WIIDEV)/bin/bootmii -a $<

clean: myclean

$(ELFLOADER):
	@$(MAKE) -C elfloader

myclean:
	-rm -f $(TARGET) $(TARGET_STRIPPED) $(TARGET_BIN)
	@$(MAKE) -C elfloader clean

