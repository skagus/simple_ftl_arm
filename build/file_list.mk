
LINK_SCRIPT = STM32F103C8TX_FLASH.ld

################ source files ##############
# Source file들은 project TOP 에서의 위치를 나타낸다.
CSRC =	\
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c \
	Core/Src/stm32f1xx_hal_msp.c \
	Core/Src/stm32f1xx_it.c \
	Core/Src/syscalls.c \
	Core/Src/sysmem.c \
	Core/Src/system_stm32f1xx.c \
	$(VER_FILE)

CPPSRC = \
	Core/Src/ftl_all/buf.cpp \
	Core/Src/ftl_all/io.cpp \
	Core/Src/ftl_all/scheduler.cpp \
	Core/Src/ftl_all/page_ftl.cpp \
	Core/Src/ftl_all/page_gc.cpp \
	Core/Src/ftl_all/page_meta.cpp \
	Core/Src/ftl_all/page_req.cpp \
	Core/Src/ftl_all/test.cpp \
	Core/Src/main.cpp

ASRC = \
	Core/Startup/startup_stm32f103c8tx.S

