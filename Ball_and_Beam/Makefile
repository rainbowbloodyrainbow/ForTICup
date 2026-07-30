SDK_ROOT ?= $(HOME)/ti/mspm0_sdk_2_11_00_07
SYSCONFIG_ROOT ?= $(HOME)/ti/sysconfig_1.28.0
SYSCONFIG_CLI = $(SYSCONFIG_ROOT)/sysconfig_cli.sh

SYSCONFIG_FILE = ballandbeam.syscfg
GEN_DIR = ti_generated

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

CFLAGS = \
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
	-D__MSPM0G3507__

INCLUDES = \
	-I$(GEN_DIR) \
	-I$(SDK_ROOT)/source \
	-I$(SDK_ROOT)/source/third_party/CMSIS/Core/Include

SOURCES = \
	main.c \
	as5600.c \
	foc_pwm.c \
	motor_control.c \
	uart_command.c \
	$(GEN_DIR)/ti_msp_dl_config.c

STARTUP_FILE = $(SDK_ROOT)/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c
DRIVERLIB = $(SDK_ROOT)/source/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a

LDFLAGS = \
	-nostartfiles \
	-T $(GEN_DIR)/device_linker.lds \
	-Wl,--gc-sections \
	-Wl,-Map,firmware.map \
	--specs=nano.specs \
	--specs=nosys.specs

.PHONY: all sysconfig syscfg build size clean

all: sysconfig build

sysconfig:
	mkdir -p $(GEN_DIR)
	$(SYSCONFIG_CLI) \
		-s $(SDK_ROOT)/.metadata/product.json \
		--compiler gcc \
		-o $(GEN_DIR) \
		$(SYSCONFIG_FILE)

syscfg: sysconfig

build:
	$(CC) $(CFLAGS) $(INCLUDES) \
		$(SOURCES) \
		$(STARTUP_FILE) \
		$(DRIVERLIB) \
		$(LDFLAGS) \
		-lm \
		-o firmware.elf
	$(OBJCOPY) -O ihex firmware.elf firmware.hex

size: firmware.elf
	$(SIZE) firmware.elf

firmware.elf: sysconfig
	$(MAKE) build

clean:
	rm -rf $(GEN_DIR)
	rm -f firmware.elf firmware.hex firmware.map
