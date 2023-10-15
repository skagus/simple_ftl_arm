
################ Top configuration.
PRJ_TOP = ..
TARGET = ftl_fsm
OBJDIR = obj

MCU = cortex-m3

BUILD_STRICT = FALSE
BUILD_PRINT = FALSE

OPTI = -O3

ifeq ($(BUILD_PRINT), TRUE)
	ECHO = 
else
	ECHO = @
endif

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

LINK_SCRIPT = STM32F103C8TX_FLASH.ld
VER_FILE = version.c
################ Object ##############
OBJ =	$(CSRC:%.c=$(OBJDIR)/%.c.o) \
		$(CPPSRC:%.cpp=$(OBJDIR)/%.p.o) \
		$(ASRC:%.S=$(OBJDIR)/%.s.o) \
		$(DATA:%.bin=$(OBJDIR)/%.b.o)

LST =	$(OBJ:%.o=%.lst)
DEP =	$(OBJ:%.o=%.d)

################  Define
DEFINE = 
# STM32F103C8T6 <-- MD performance.
DEFINE += -DSTM32F103xB
DEFINE += -DUSE_HAL_DRIVER
DEFINE += -DDEBUG

################  Library directory.
PRJ_LIB_DIR =

EXT_LIB_DIR = 

LIB_FILE =

LIB_OPT =	$(patsubst %,-L$(PRJ_TOP)/%,$(PRJ_LIB_DIR)) \
			$(patsubst %,-L%,$(EXT_LIB_DIR)) \
			$(patsubst %,-l%,$(LIB_FILE))

################  Include.
# Add relative path from $(PRJ_TOP)
PRJ_INC = \
	Core/Inc \
	Drivers/STM32F1xx_HAL_Driver/Inc/Legacy \
	Drivers/STM32F1xx_HAL_Driver/Inc \
	Drivers/CMSIS/Device/ST/STM32F1xx/Include \
	Drivers/CMSIS/Include


# Add absolue path. (ex. c:/lib/inc)
EXT_INC =

INCLUDE =	$(patsubst %,-I$(PRJ_TOP)/%,$(PRJ_INC)) \
			$(patsubst %,-I%,$(EXT_INC))



################  Common Options for C, C++, ASM
#  -g*:          generate debugging information
#  -O*:          optimization level
#  -f...:        tuning, see GCC manual and avr-libc documentation
#  -Wall...:     warning level
#  -Wa,...:      tell GCC to pass this to the assembler.
#    -adhlns...: create assembler listing
FLAGS  = -mcpu=$(MCU)
#FLAGS += -mthumb
FLAGS += -Wa,-adhlns=$(@:.o=.lst) # .o file assembly.
FLAGS += -gdwarf-2 # debug format: dwarf-2
FLAGS += -MMD -MP # dependancy generation.

## Optimize ##
FLAGS += $(OPTI)

## compiler configuration ##
FLAGS += -funsigned-char
FLAGS += -funsigned-bitfields
FLAGS += -fshort-enums
FLAGS += -fmessage-length=0 # error message in a line (w/o wrap)
FLAGS += -ffunction-sections
FLAGS += -fdata-sections 
#FLAGS += -fpack-struct  # CAUTION: RISC-V can't handle un-aligned load/save.
#FLAGS += -msave-restore 
#FLAGS += -fno-unit-at-a-time
#FLAGS += -mshort-calls
FLAGS += --specs=nano.specs
FLAGS += -mfloat-abi=soft
## WARNING ##
FLAGS += -Wcomment # enable warning for cascade comment.
FLAGS += -Wunreachable-code
ifeq ($(BUILD_STRICT),TRUE)
	FLAGS += -Wall
	FLAGS += -Wextra
	FLAGS += -Wstrict-prototypes
	#CFLAGS += -Wundef # for undefined macro evaluation
endif

################  C Options
CFLAGS  = $(FLAGS)
CFLAGS += -std=gnu11 # c89, c99, gnu89, gnu99
CFLAGS += -fstack-usage  # show stack usage for each function.
CFLAGS += -fcallgraph-info  # make call graph information.
CFLAGS += $(DEP_OPT)
CFLAGS += $(DEFINE)
CFLAGS += $(INCLUDE)
#CFLAGS += -fdump-tree-optimized #
#CFLAGS += -fdump-rtl-dfinish  # 
################  C++ Options
CPPFLAGS  = $(FLAGS)
CPPFLAGS += -std=gnu++14
CPPFLAGS += -fstack-usage  # show stack usage for each function.
CPPFLAGS += -fcallgraph-info  # make call graph information.
CPPFLAGS += -fno-exceptions
CPPFLAGS += -fno-rtti
CPPFLAGS += -fno-use-cxa-atexit
CPPFLAGS += $(DEP_OPT)
CPPFLAGS += $(DEFINE)
CPPFLAGS += $(INCLUDE)

################  Assembler Options
#  -Wa,...:   tell GCC to pass this to the assembler.
#  -gstabs:   have the assembler create line number information; note that
#             for use in COFF files, additional information about filenames
#             and function names needs to be present in the assembler source
#             files -- see avr-libc docs [FIXME: not yet described there]
#  -listing-cont-lines: Sets the maximum number of continuation lines of hex 
#       dump that will be displayed for a given single line of source input.
ASFLAGS  = $(FLAGS)
#ASFLAGS += -Wa,-gstabs,--listing-cont-lines=100
ASFLAGS += $(DEP_OPT)
ASFLAGS += $(DEFINE)
ASFLAGS += $(INCLUDE)
ASFLAGS += -x assembler-with-cpp 

################  Linker Options
#  -Wl,...:     tell GCC to pass this to linker.
#    -Map:      create map file
#    --cref:    add cross reference to  map file
LDS_OPT = $(patsubst %, -T $(PRJ_TOP)/%, $(LINK_SCRIPT))

LDFLAGS = -g
LDFLAGS += -mcpu=$(MCU)
LDFLAGS += $(OPTI)
#LDFLAGS += -static  
LDFLAGS += -Xlinker -Map=$(TARGET).map
LDFLAGS += -Xlinker --cref
#LDFLAGS += -Wl,-Map=$(TARGET).map,--cref
LDFLAGS += $(LDS_OPT)
#LDFLAGS += --specs=nosys.specs
LDFLAGS += --specs=nano.specs
#LDFLAGS += -nostartfiles
#LDFLAGS += -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group
#LDFLAGS += -nodefaultlibs
#LDFLAGS += -nolibc
#LDFLAGS += -nostdlib
LDFLAGS += -Xlinker --gc-sections
LDFLAGS += -Xlinker --print-memory-usage
#LDFLAGS += -mcmodel=medany
#LDFLAGS += -msmall-data-limit=8
LDFLAGS += -fmessage-length=0
LDFLAGS += -mfloat-abi=soft


#-Xlinker --cref  -nostartfiles   -Xlinker --print-memory-usage -fmessage-length=0

################### Action #################
# add @ for silent.
GNU_PRIFIX = arm-none-eabi-

CC 		= $(GNU_PRIFIX)gcc
CPP 	= $(GNU_PRIFIX)g++
OBJCOPY	= $(GNU_PRIFIX)objcopy
OBJDUMP	= $(GNU_PRIFIX)objdump
SIZE	= $(GNU_PRIFIX)size
AR		= $(GNU_PRIFIX)ar rcs
NM		= $(GNU_PRIFIX)nm

SHELL = sh
REMOVE = rm -f
REMOVEDIR = rm -rf
COPY = cp

INFO 	= @echo Making: $@
DIR_CHK = @mkdir -p $(@D)

################ Object file
$(OBJDIR)/%.b.o : $(PRJ_TOP)/%.bin
	$(INFO)
	$(DIR_CHK)
	$(OBJCOPY) -I binary -O elf32 $< $@

$(OBJDIR)/%.c.o : $(PRJ_TOP)/%.c
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(CC) -c $(CFLAGS) $< -o $@ 

$(OBJDIR)/%.p.o : $(PRJ_TOP)/%.cpp
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(CPP) -c $(CPPFLAGS) $< -o $@ 

$(OBJDIR)/%.s.o : $(PRJ_TOP)/%.S
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(CC) -c $(ASFLAGS) $< -o $@

################### File Creation #################
all: elf bin lst hex size

relink: clean_link all

$(PRJ_TOP)/$(VER_FILE): Makefile
	$(INFO)
	@echo "char* gpVersion = \"$(VER_STRING)\";" > $(PRJ_TOP)/$(VER_FILE)

%.elf: $(OBJ)
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(CC) $(LDFLAGS) $^ $(LIB_OPT) -o $@ 

%.hex: %.elf
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(OBJCOPY) -O ihex  $< $@

%.bin: %.elf
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(OBJCOPY) -O binary $< $@

%.lss: %.elf
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(OBJDUMP) -h -S -z $< > $@

%.sym: %.elf
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(NM) -S -l -n --format=sysv $< > $@

%.a: $(OBJ)
	$(INFO)
	$(DIR_CHK)
	$(ECHO)$(AR) $@ $^

################ Actions.
TARGET_ALL=	$(TARGET).elf $(TARGET).a \
			$(TARGET).hex $(TARGET).bin \
			$(TARGET).lss $(TARGET).sym $(TARGET).map

VER_STRING = $$(date +%Y%m%d_%H%M%S)_$(OPTI)

version:
	@echo "char* gpVersion = \"$(VER_STRING)\";" > $(PRJ_TOP)/$(VER_FILE)
#	@echo "char* gpVersion = \"$$(date_time)\";" > $(PRJ_TOP)/$(VER_FILE)
#	@echo "char* gpVersion = \"$$(date +%Y%m%d_%H%M%S)\";" > $(PRJ_TOP)/$(VER_FILE)

obj: $(OBJ)
lib: $(TARGET).a
elf: $(TARGET).elf
bin: $(TARGET).bin $(TARGET).hex
lst: $(TARGET).lss $(TARGET).sym

size: $(TARGET).elf
	$(INFO)
	@$(SIZE) --format=gnu $<
	@$(SIZE) --format=gnu $< > $(TARGET).size
	@$(SIZE) --format=sysv --radix=16 --common $< >> $(TARGET).size

prog: $(TARGET).hex
	st-link_cli -P $(TARGET).hex -Rst
#	stm32_isp.exe stm32_conf.ini

run:
	st-link_cli -Rst

# Display compiler version information.
gccversion : 
	$(ECHO)$(CC) --version

clean: clean_link
	$(REMOVEDIR) $(OBJDIR)

clean_link:
	$(REMOVE) $(TARGET).elf $(TARGET).a \
			$(TARGET).hex $(TARGET).bin \
			$(TARGET).lss $(TARGET).sym \
			$(TARGET).map $(TARGET).size

# Include the dependency files.
-include $(patsubst %.c,$(OBJDIR)/%.c.d,$(CSRC))
-include $(patsubst %.cpp,$(OBJDIR)/%.p.d,$(CPPSRC))
-include $(patsubst %.S,$(OBJDIR)/%.S.d,$(ASRC))

# Listing of phony targets.
.PHONY : all obj elf bin hex lst size gccversion clean version
