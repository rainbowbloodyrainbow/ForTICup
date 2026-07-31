SDK_ROOT ?= /home/clbritheking/ti/mspm0_sdk_2_11_00_07
SYSCONFIG_ROOT ?= /home/clbritheking/ti/sysconfig_1.28.0

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size
SYSCONFIG_CLI := $(SYSCONFIG_ROOT)/sysconfig_cli.sh

SYSCONFIG_FILE := qei_test.syscfg
GEN_DIR := generated
BUILD_DIR := build
GEN_STAMP := $(GEN_DIR)/.generated
ELF := $(BUILD_DIR)/qei_test.elf
HEX := $(BUILD_DIR)/qei_test.hex

CFLAGS := \
	-std=c11 \
	-mcpu=cortex-m0plus \
	-mthumb \
	-O2 \
	-g \
	-ffunction-sections \
	-fdata-sections \
	-Wall \
	-Wextra \
	-Werror \
	-D__MSPM0G3507__ \
	-I$(GEN_DIR) \
	-I$(SDK_ROOT)/source \
	-I$(SDK_ROOT)/source/third_party/CMSIS/Core/Include

STARTUP := $(SDK_ROOT)/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c
DRIVERLIB := $(SDK_ROOT)/source/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a

LDFLAGS := \
	-nostartfiles \
	-T $(GEN_DIR)/device_linker.lds \
	-Wl,--gc-sections \
	-Wl,-Map,$(BUILD_DIR)/qei_test.map \
	--specs=nano.specs \
	--specs=nosys.specs

.PHONY: all sysconfig size clean

all: $(HEX) size

sysconfig: $(GEN_STAMP)

$(GEN_STAMP): $(SYSCONFIG_FILE)
	mkdir -p $(GEN_DIR)
	$(SYSCONFIG_CLI) \
		-s $(SDK_ROOT)/.metadata/product.json \
		--compiler gcc \
		-o $(GEN_DIR) \
		$(SYSCONFIG_FILE)
	touch $(GEN_STAMP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ELF): main.c $(GEN_STAMP) | $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		main.c \
		$(GEN_DIR)/ti_msp_dl_config.c \
		$(STARTUP) \
		$(DRIVERLIB) \
		$(LDFLAGS) \
		-o $(ELF)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $(ELF) $(HEX)

size: $(ELF)
	$(SIZE) $(ELF)

clean:
	rm -rf $(GEN_DIR) $(BUILD_DIR)
