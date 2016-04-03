/*
 * REPTAR FPGA buttons emulation
 *
 * Copyright (c) 2013 Reconfigurable Embedded Digital Systems (REDS) Institute at HEIG-VD, Switzerland
 * Written by Romain Bornet <romain.bornet@heig-vd.ch>
 *
 * This module provides a basic emulation for the 8 buttons of REPTAR's FPGA board.
 *
 */

#include "reptar_sp6.h"

sp6_state_t *sp6_state;
int btnStatus;

int reptar_sp6_btns_event_process(cJSON * root)
{
	if(root != NULL)
	{
		btnStatus = cJSON_GetObjectItem(root,"status")->valueint;
		printf("Button status : 0x%x\n",btnStatus);
		cJSON_Delete(root);
	}
	return btnStatus;
}

int reptar_sp6_btns_init(void *opaque)
{
    sp6_state = (sp6_state_t *) opaque;

    return 0;
}
