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
		
		// Raise IRQ only if irq are allowed and button pressed (not 0x00 to avoid rebond)
		if(sp6_state->irq_enabled && btnStatus > 0)
		{
			printf("IRQ RAISE\n");
			sp6_state->irq_pending = 1;
			qemu_irq_raise(sp6_state->irq);
			
		}
	}
	return btnStatus;
}

int reptar_sp6_btns_init(void *opaque)
{
    sp6_state = (sp6_state_t *) opaque;

    return 0;
}
