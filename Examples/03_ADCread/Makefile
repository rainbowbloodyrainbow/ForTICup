# ==========================================
# 飞鱼项目 MSPM0G3507 极简自动化构建脚本
# ==========================================

# 1. 核心路径定义 (请确保 SDK_ROOT 路径正确)
SDK_ROOT = $(HOME)/ti/mspm0_sdk_2_11_00_07
SYSCFG_CLI = $(HOME)/ti/sysconfig_1.28.0/sysconfig_cli.sh

# 2. 工程文件定义
SYSCFG_FILE = 03_ADCread.syscfg
GEN_DIR = ./ti_generated

# 3. 交叉编译器工具链
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

# 4. 编译与链接参数
CFLAGS = -mcpu=cortex-m0plus -mthumb -O2 -g -D__MSPM0G3507__
INCLUDES = \
	-I$(GEN_DIR) \
	-I../../modules/adc \
	-I../../modules/hc05 \
	-I$(SDK_ROOT)/source \
	-I$(SDK_ROOT)/source/third_party/CMSIS/Core/Include

# 修复后的 TI 官方启动文件与底层驱动库路径
STARTUP_FILE = $(SDK_ROOT)/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c
DRIVERLIB = $(SDK_ROOT)/source/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a

# 链接参数 (直接挂载 SysConfig 自动生成的链接脚本)
LDFLAGS = \
	-nostartfiles \
	-T $(GEN_DIR)/device_linker.lds \
	-Wl,--gc-sections \
	-Wl,-Map,firmware.map \
	--specs=nano.specs \
	--specs=nosys.specs
.PHONY: all syscfg build clean

# 终极目标：先生成配置，再编译固件
all: syscfg build
	@echo "=================================================="
	@echo "🎉 Build complete! firmware.hex is ready."
	@echo "=================================================="

# 步骤 A：无头调用 SysConfig 生成底层驱动代码
syscfg:
	@echo ">>> Generating hardware config from SysConfig..."
	@mkdir -p $(GEN_DIR)
	$(SYSCFG_CLI) -s $(SDK_ROOT)/.metadata/product.json --compiler gcc -o $(GEN_DIR) $(SYSCFG_FILE)

# 步骤 B：调用 ARM-GCC 交叉编译
build:
	@echo ">>> Compiling ADC.c and generated files..."
	$(CC) $(CFLAGS) $(INCLUDES) \
		ADC.c \
		../../modules/adc/adc.c \
		../../modules/hc05/hc05.c \
		$(GEN_DIR)/ti_msp_dl_config.c \
		$(STARTUP_FILE) \
		$(DRIVERLIB) \
		$(LDFLAGS) \
		-o firmware.elf
	@echo ">>> Generating firmware.hex..."
	$(OBJCOPY) -O ihex firmware.elf firmware.hex

clean:
	@echo ">>> Cleaning workspace..."
	rm -rf $(GEN_DIR) *.elf *.hex
