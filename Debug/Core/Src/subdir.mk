################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/debug_output.c \
../Core/Src/feedback.c \
../Core/Src/flight_logger.c \
../Core/Src/main.c \
../Core/Src/sdp810_driver.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/storage_logger.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32h7xx.c \
../Core/Src/timebase_us.c \
../Core/Src/tube_profile.c 

OBJS += \
./Core/Src/debug_output.o \
./Core/Src/feedback.o \
./Core/Src/flight_logger.o \
./Core/Src/main.o \
./Core/Src/sdp810_driver.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/storage_logger.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32h7xx.o \
./Core/Src/timebase_us.o \
./Core/Src/tube_profile.o 

C_DEPS += \
./Core/Src/debug_output.d \
./Core/Src/feedback.d \
./Core/Src/flight_logger.d \
./Core/Src/main.d \
./Core/Src/sdp810_driver.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/storage_logger.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32h7xx.d \
./Core/Src/timebase_us.d \
./Core/Src/tube_profile.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H723xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/debug_output.cyclo ./Core/Src/debug_output.d ./Core/Src/debug_output.o ./Core/Src/debug_output.su ./Core/Src/feedback.cyclo ./Core/Src/feedback.d ./Core/Src/feedback.o ./Core/Src/feedback.su ./Core/Src/flight_logger.cyclo ./Core/Src/flight_logger.d ./Core/Src/flight_logger.o ./Core/Src/flight_logger.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/sdp810_driver.cyclo ./Core/Src/sdp810_driver.d ./Core/Src/sdp810_driver.o ./Core/Src/sdp810_driver.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/storage_logger.cyclo ./Core/Src/storage_logger.d ./Core/Src/storage_logger.o ./Core/Src/storage_logger.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32h7xx.cyclo ./Core/Src/system_stm32h7xx.d ./Core/Src/system_stm32h7xx.o ./Core/Src/system_stm32h7xx.su ./Core/Src/timebase_us.cyclo ./Core/Src/timebase_us.d ./Core/Src/timebase_us.o ./Core/Src/timebase_us.su ./Core/Src/tube_profile.cyclo ./Core/Src/tube_profile.d ./Core/Src/tube_profile.o ./Core/Src/tube_profile.su

.PHONY: clean-Core-2f-Src

