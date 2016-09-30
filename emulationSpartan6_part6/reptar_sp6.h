/*
 * REPTAR Spartan6 FPGA emulation
 *
 * Copyright (c) 2013 HEIG-VD / REDS
 * Written by Romain Bornet
 *
 * This code is licensed under the GPL.
 */

#ifndef HW_SP6_H
#define HW_SP6_H

#include "hw/sysbus.h"
#include "qemu-common.h"
#include "reptar_sp6_buttons.h"
#include "reptar_sp6_clcd.h"
#include "reptar_sp6_emul.h"

typedef struct
{
    SysBusDevice busdev;
    MemoryRegion iomem;
    uint16_t regs[512];		/* 1KB (512 * 16bits registers) register map */

    qemu_irq irq;
    int irq_pending;
    int irq_enabled;
} sp6_state_t;

/* Implemented registers addresses */
#define SP6_IRQ_STATUS      0x0010
#define SP6_PUSH_BUT		0x0012
#define SP6_IRQ_CTL			0x0018
#define SP6_7SEG1			0x0030
#define SP6_7SEG2			0x0032
#define SP6_7SEG3			0x0034
#define SP6_LED				0x003A
#define SP6_LCD_CONTROL		0x0036
#define SP6_LCD_STATUS		0x0038

/* Constants */
#define SP6_IRQ_CLEAR       0x0001
#define SP6_IRQ_EN          0x0080
#define SP6_IRQ_BTNS_MASK   0x000E

/* Debug output configuration #define or #undef */
#define REPTAR_SP6_EMUL_DEBUG 1

#ifdef REPTAR_SP6_EMUL_DEBUG
#define DBG(fmt, ...) \
    do { printf("reptar-sp6-emul: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DBG(fmt, ...) \
    do { } while (0)
#endif

#endif /* #ifndef HW_SP6_H */
