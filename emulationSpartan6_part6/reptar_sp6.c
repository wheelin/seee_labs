#include "qemu-common.h"
#include "reptar_sp6.h"
#include "hw/sysbus.h"

#include "cJSON.h"

#define SP6_BASE_ADDRESS	0x18000000

uint16_t 			led_register_value;
uint16_t			button_register_value;
uint16_t			disp_7seg_d1_register_value;
uint16_t			disp_7seg_d2_register_value;
uint16_t			disp_7seg_d3_register_value;
uint16_t			button_irq_register_value;

static uint64_t sp6_read(void *opaque, hwaddr offset, unsigned size)
{
	sp6_state_t *s = (sp6_state_t *)opaque;
	switch(offset)
	{
		case SP6_LED:
			if(size == 2)
			{
				printf ("sp6_read: Led read 0x%x\n",led_register_value);
				return led_register_value;
			}
			printf ("sp6_read: Led read wrong data size %d should be 2\n",size);
			return 0;
		break;
		case SP6_PUSH_BUT:
			if(size == 2)
			{
				button_register_value = reptar_sp6_btns_event_process(NULL);
				printf("sp6_read: Button read 0x%x\n",button_register_value);
				return button_register_value;
			}
			printf ("sp6_read: Button read wrong data size %d should be 2\n",size);
			return 0;
		break;
		case SP6_7SEG1:
			if(size == 2)
			{
				printf ("sp6_read: Display 7 segments D1 read 0x%x\n",disp_7seg_d1_register_value);
				return disp_7seg_d1_register_value;
			}
			printf ("sp6_read: Display 7 segments D1 read wrong data size %d should be 2\n",size);
			return 0;
		break;
		case SP6_7SEG2:
			if(size == 2)
			{
				printf ("sp6_read: Display 7 segments D2 read 0x%x\n",disp_7seg_d2_register_value);
				return disp_7seg_d2_register_value;
			}
			printf ("sp6_read: Display 7 segments D2 read wrong data size %d should be 2\n",size);
			return 0;
		break;
		case SP6_7SEG3:
			if(size == 2)
			{
				printf ("sp6_read: Display 7 segments D3 read 0x%x\n",disp_7seg_d3_register_value);
				return disp_7seg_d3_register_value;
			}
			printf ("sp6_read: Display 7 segments D3 read wrong data size %d should be 2\n",size);
			return 0;
		break;
		case SP6_IRQ_CTL:
			if(size == 2)
			{
				uint8_t temp = 0;
				
				//Read button register value
				button_register_value = reptar_sp6_btns_event_process(NULL);
				
				//Convert button bit to its position decimal value
				switch(button_register_value)
				{
					case 0x01:
						temp = 0;
					break;
					case 0x02:
						temp = 1;
					break;
					case 0x04:
						temp = 2;
					break;
					case 0x08:
						temp = 3;
					break;
					case 0x10:
						temp = 4;
					break;
					case 0x20:
						temp = 5;
					break;
					case 0x40:
						temp = 6;
					break;
					case 0x80:
						temp = 7;
					break;
					default:
						temp = 0;
					break;
				}
				button_irq_register_value = button_irq_register_value | (((temp)<<1)&SP6_IRQ_BTNS_MASK);
				printf ("sp6_read: Button irq status read 0x%x (button %d made IRQ)\n",button_irq_register_value,temp+1);
				button_irq_register_value |= (1 << 4);
				return button_irq_register_value;
			}
			printf ("sp6_read: Button irq status read wrong data size %d should be 2\n",size);
			return 0;
		break;
		default:
			printf ("sp6_read: Bad register offset 0x%x\n", (int)offset);
			return 0;
		break;
	}
	return 0;
}

static void sp6_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
	sp6_state_t *s = (sp6_state_t *)opaque;

	cJSON *root;
	switch(offset)
	{
		case SP6_LED:
			if(size == 2)
			{
				printf ("sp6_write: Led write 0x%x\n",value);
				led_register_value = (uint16_t) value;
				//Create JSON response
				root = cJSON_CreateObject();
				cJSON_AddStringToObject(root,"perif","led");
				cJSON_AddNumberToObject(root,"value",led_register_value);
				sp6_emul_cmd_post(root);
			}
			else
				printf ("sp6_write: Led write wrong data size %d should be 2\n",size);
		break;
		case SP6_7SEG1:
			if(size == 2)
			{
				printf ("sp6_write: Display 7 segments D1 write 0x%x\n",value);
				disp_7seg_d1_register_value = (uint16_t) value;
				//Create JSON response
				root = cJSON_CreateObject();
				cJSON_AddStringToObject(root,"perif","7seg");
				cJSON_AddNumberToObject(root,"digit",1);
				cJSON_AddNumberToObject(root,"value",disp_7seg_d1_register_value);
				sp6_emul_cmd_post(root);
			}
			else
				printf ("sp6_write: Display 7 segments D1 write wrong data size %d should be 2\n",size);
		break;
		case SP6_7SEG2:
			if(size == 2)
			{
				printf ("sp6_write: Display 7 segments D2 write 0x%x\n",value);
				disp_7seg_d2_register_value = (uint16_t) value;
				//Create JSON response
				root = cJSON_CreateObject();
				cJSON_AddStringToObject(root,"perif","7seg");
				cJSON_AddNumberToObject(root,"digit",2);
				cJSON_AddNumberToObject(root,"value",disp_7seg_d2_register_value);
				sp6_emul_cmd_post(root);
			}
			else
				printf ("sp6_write: Display 7 segments D2 write wrong data size %d should be 2\n",size);
		break;
		case SP6_7SEG3:
			
			if(size == 2)
			{
				printf ("sp6_write: Display 7 segments D3 write 0x%x\n",value);
				disp_7seg_d3_register_value = (uint16_t) value;
				//Create JSON response
				root = cJSON_CreateObject();
				cJSON_AddStringToObject(root,"perif","7seg");
				cJSON_AddNumberToObject(root,"digit",3);
				cJSON_AddNumberToObject(root,"value",disp_7seg_d3_register_value);
				sp6_emul_cmd_post(root);
			}
			else
				printf ("sp6_write: Display 7 segments D3 write wrong data size %d should be 2\n",size);
		break;
		case SP6_IRQ_CTL:
			printf ("sp6_write: Button irq status write 0x%x\n",value);
			button_irq_register_value = (uint16_t) value;
			
			//Parse writed value to enable and ack interrupt
			if((button_irq_register_value & SP6_IRQ_EN) > 0)
			{
				printf ("Enable IRQ\n");
				s->irq_enabled = 1;
			}
			else
			{
				printf ("Disable IRQ\n");
				s->irq_enabled = 0;
			}
			if((button_irq_register_value & SP6_IRQ_CLEAR) > 0)
			{
				printf ("Clear IRQ\n");
				s->irq_pending = 0;
				qemu_irq_lower(s->irq);
			}
		break;
		default:
			printf ("sp6_write: Bad register offset 0x%x\n", (int)offset);
		break;
	}
}

static const MemoryRegionOps reptar_sp6_ops = {
    .read = sp6_read,
    .write = sp6_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int sp6_initfn(SysBusDevice *dev)
{
	printf("sp6 initfn\n");
	DeviceState *sbd = DEVICE(dev);
	sp6_state_t *s = OBJECT_CHECK(sp6_state_t, dev, "reptar_sp6");
	memory_region_init_io(&s->iomem, OBJECT(s), &reptar_sp6_ops, s,
                          "reptar_sp6", 0x100000);
    	sysbus_init_mmio(sbd, &s->iomem);

	sysbus_init_irq(sbd, &s->irq);
	
	// Init sp6_buttons
	reptar_sp6_btns_init((void*)s);

	return 0;
}

static void sp6_class_init(ObjectClass *this, void *data)
{
	printf("sp6 class init\n");
	SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(this);
	k->init = sp6_initfn;
}

static void sp6_init(void)
{
	printf("sp6 init\n");
	sp6_emul_init();

	led_register_value = 0x0000;
}

static const TypeInfo reptar_sp6_info = {
	.name = "reptar_sp6",
	.parent = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(sp6_state_t),
	.instance_init = sp6_init,
	.class_init = sp6_class_init,
};

static void sp6_register_types(void)
{
	printf("sp6 register types\n");
	type_register_static(&reptar_sp6_info);
}

type_init(sp6_register_types);
