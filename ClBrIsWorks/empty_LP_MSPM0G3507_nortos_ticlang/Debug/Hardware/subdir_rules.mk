################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
Hardware/%.o: ../Hardware/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"D:/ti/ccs2020/ccs/tools/compiler/ti-cgt-armllvm_4.0.3.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O0 -I"C:/Users/Mayn/Desktop/42+encode/CCS/empty_LP_MSPM0G3507_nortos_ticlang/BSP/MPU6050" -I"C:/Users/Mayn/Desktop/42+encode/CCS/empty_LP_MSPM0G3507_nortos_ticlang/BSP/MPU6050/DMP" -I"C:/Users/Mayn/Desktop/42+encode/CCS/empty_LP_MSPM0G3507_nortos_ticlang/CONTROL" -I"C:/Users/Mayn/Desktop/42+encode/CCS/empty_LP_MSPM0G3507_nortos_ticlang/HARDWARE" -I"C:/Users/Mayn/Desktop/42+encode/CCS/empty_LP_MSPM0G3507_nortos_ticlang" -I"C:/Users/Mayn/Desktop/42+encode/CCS/empty_LP_MSPM0G3507_nortos_ticlang/Debug" -I"D:/ti/mspm0_sdk_2_01_00_03/source/third_party/CMSIS/Core/Include" -I"D:/ti/mspm0_sdk_2_01_00_03/source" -gdwarf-3 -MMD -MP -MF"Hardware/$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


