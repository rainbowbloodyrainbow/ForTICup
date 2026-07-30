SDK_ROOT ?= $(HOME)/ti/mspm0_sdk_2_11_00_07
SYSCONFIG_ROOT ?= $(HOME)/ti/sysconfig_1.28.0
SYSCONFIG_CLI = $(SYSCONFIG_ROOT)/sysconfig_cli.sh

SYSCONFIG_FILE = all.syscfg
GEN_DIR = 00_generated
BUILD_DIR = build
HOST_BUILD_DIR = $(BUILD_DIR)/host

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size
HOST_CC ?= gcc

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
	-Imyownlib/01_platform/adc \
	-Imyownlib/01_platform/output \
	-Imyownlib/01_platform/system_time \
	-Imyownlib/02_device/hc05 \
	-Imyownlib/02_device/line_sensor \
	-Imyownlib/02_device/motor \
	-Imyownlib/02_device/mpu6050 \
	-Imyownlib/02_device/servo \
	-Imyownlib/03_algorithm/pid \
	-Imyownlib/04_control/line_control \
	-Imyownlib/05_robot/chassis \
	-Imyownlib/07_application/application \
	-I$(SDK_ROOT)/source \
	-I$(SDK_ROOT)/source/third_party/CMSIS/Core/Include

SOURCES = \
	main.c \
	myownlib/01_platform/adc/adc.c \
	myownlib/01_platform/output/output.c \
	myownlib/01_platform/system_time/system_time.c \
	myownlib/02_device/hc05/hc05.c \
	myownlib/02_device/line_sensor/line_sensor.c \
	myownlib/02_device/motor/motor.c \
	myownlib/02_device/mpu6050/mpu6050.c \
	myownlib/02_device/servo/servo.c \
	myownlib/03_algorithm/pid/pid.c \
	myownlib/04_control/line_control/line_control.c \
	myownlib/05_robot/chassis/chassis.c \
	myownlib/07_application/application/application.c \
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

.PHONY: all sysconfig syscfg build clean size host-test

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
		-o firmware.elf
	$(OBJCOPY) -O ihex firmware.elf firmware.hex

size: firmware.elf
	$(SIZE) firmware.elf

firmware.elf: sysconfig
	$(MAKE) build

host-test:
	mkdir -p $(HOST_BUILD_DIR)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-Imyownlib/01_platform/system_time \
		tests/host/system_time/test_system_time.c \
		myownlib/01_platform/system_time/system_time.c \
		-o $(HOST_BUILD_DIR)/test_system_time
	$(HOST_BUILD_DIR)/test_system_time
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-Imyownlib/02_device/line_sensor \
		tests/host/line_sensor/test_line_sensor.c \
		myownlib/02_device/line_sensor/line_sensor.c \
		-o $(HOST_BUILD_DIR)/test_line_sensor
	$(HOST_BUILD_DIR)/test_line_sensor
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-Imyownlib/03_algorithm/pid \
		tests/host/pid/test_pid.c \
		myownlib/03_algorithm/pid/pid.c \
		-lm \
		-o $(HOST_BUILD_DIR)/test_pid
	$(HOST_BUILD_DIR)/test_pid
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-Imyownlib/02_device/line_sensor \
		-Imyownlib/03_algorithm/pid \
		-Imyownlib/04_control/line_control \
		tests/host/line_control/test_line_control.c \
		myownlib/04_control/line_control/line_control.c \
		myownlib/03_algorithm/pid/pid.c \
		-lm \
		-o $(HOST_BUILD_DIR)/test_line_control
	$(HOST_BUILD_DIR)/test_line_control
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-Itests/host/motor/fakes \
		-Imyownlib/02_device/motor \
		tests/host/motor/test_motor.c \
		myownlib/02_device/motor/motor.c \
		-o $(HOST_BUILD_DIR)/test_motor
	$(HOST_BUILD_DIR)/test_motor
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-Itests/host/chassis/fakes \
		-Imyownlib/02_device/motor \
		-Imyownlib/05_robot/chassis \
		tests/host/chassis/test_chassis.c \
		myownlib/02_device/motor/motor.c \
		myownlib/05_robot/chassis/chassis.c \
		-o $(HOST_BUILD_DIR)/test_chassis
	$(HOST_BUILD_DIR)/test_chassis

clean:
	rm -rf $(GEN_DIR) $(BUILD_DIR)
	rm -f firmware.elf firmware.hex firmware.map
