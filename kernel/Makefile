TARGET = riscv64-ls-elf


GCC = $(TARGET)-gcc
OBJDUMP	= $(TARGET)-objdump
AR		= $(TARGET)-ar


PROG	= riscv-spike
CRT0	= arch/boot.S

FREERTOS_SOURCE_DIR	= ../kernel

SCHEDULE = ../schedule/src
TIMER = ../Softtime/src
MEMORY = ../Memory/src
COMMON = ../Common/src


WARNINGS= -Wall -Wextra -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-align -Wsign-compare \
		-Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wunused

INCLUDES = \
	-I. \
	-I../Common/include \
	-I../Memory/include \
	-I../schedule/include \
	-I../Softtime/include \



FREERTOS_SRC = \
	$(COMMON)/list.c \
	$(COMMON)/queue.c \
	$(SCHEDULE)/task.c \
	$(TIMER)/time.c \
	$(MEMORY)/heap.c \
	$(COMMON)/common.c \
	$(FREERTOS_SOURCE_DIR)/main.c



PORT_ASM =	\
	$(FREERTOS_SOURCE_DIR)/boot.S  \
	$(FREERTOS_SOURCE_DIR)/Content.S


CFLAGS = \
	$(WARNINGS) $(INCLUDES) \
	-fomit-frame-pointer -fno-strict-aliasing -fno-builtin \
	-D__gracefulExit

RTOS_OBJ = $(FREERTOS_SRC:.c=.o)
PORT_ASM_OBJ = $(PORT_ASM:.S=.o)

OBJS = $(RTOS_OBJ) $(PORT_ASM_OBJ)

LDFLAGS	 = -T ./link.ld -nostartfiles -static

%.o: %.c
	@echo "    CC $<"
	$(GCC)  -c $(CFLAGS) -o $@ $<

%.o: %.S
	@echo "    CC $<"
	$(GCC)  -c $(CFLAGS) -o $@ $<

all: $(PROG).elf

$(PROG).elf  : $(OBJS) Makefile 
	@echo Linking....
	@$(GCC) -o $@ $(LDFLAGS) $(OBJS) 
	@$(OBJDUMP) -S $(PROG).elf > $(PROG).dump	
	@echo Completed $@

clean :
	@rm -f $(OBJS)
	@rm -f $(PROG).elf 
	@rm -f $(PROG).dump
	@rm $(RTOS_OBJ)	-rf 
	@rm $(PORT_ASM_OBJ) -rf


sim:
	spike -d -m0x10000000:0x100000 $(PROG).elf