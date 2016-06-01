##########################################################################
# Simple Makefile for out-of-tree build of a kernel module
#
NM		= $(TOOL_PREFIX)-nm
AR		= $(TOOL_PREFIX)-ar
LD		= $(TOOL_PREFIX)-ld
AS		= $(TOOL_PREFIX)-as
CC		= $(TOOL_PREFIX)-gcc
STRIP		= $(TOOL_PREFIX)-strip
OBJCOPY		= $(TOOL_PREFIX)-objcopy
OBJDUMP		= $(TOOL_PREFIX)-objdump

TOOL_PREFIX	:= arm-linux-gnueabihf

# Module will be named sp6.ko
obj-m := sp6.o 
sp6-objs = reptar_sp6.o reptar_sp6_leds.o reptar_sp6_buttons.o

#KDIR	= ../linux-3.0-reptar
KDIR	= ../embeddedxen/linux-3.0-reptar-dom0

PWD	= $(shell pwd)

all: kernel buttons_test usertest

buttons_test: buttons_test.c
	$(CC) -marm -I$(KDIR) -static buttons_test.c -o buttons_test
  
kernel:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=$(TOOL_PREFIX)- 

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=$(TOOL_PREFIX)- clean
	rm -f *.o
	rm -f *.ko
	rm -f buttons_test
	rm -f usertest
	rm -f Module.markers
	rm -f modules.order 