/*
 * REPTAR FPGA buttons emulation
 *
 * Copyright (c) 2013 Reconfigurable Embedded Digital Systems (REDS) Institute at HEIG-VD, Switzerland
 * Written by Romain Bornet <romain.bornet@heig-vd.ch>
 *
 * This module provides a basic emulation for the 8 buttons of REPTAR's FPGA board.
 *
 */


#ifndef REPTAR_SP6_BTNS_H
#define REPTAR_SP6_BTNS_H

#include "cJSON.h"

int reptar_sp6_btns_init(void *opaque);
int reptar_sp6_btns_event_process(cJSON * packet);

#endif /* REPTAR_SP6_BTNS_H */
