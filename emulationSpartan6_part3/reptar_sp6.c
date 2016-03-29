/*
 * REPTAR Spartan6 FPGA emulation (reptar_sp6)
 *
 * Copyright (c) 2013-2014 HEIG-VD / REDS
 * Written by Romain Bornet
 *
 * This code is licensed under the GPL.
 */

#include "qemu-common.h"
#include "reptar_sp6.h"
#include "hw/sysbus.h"

#include "cJSON.h"

#define SP6_BASE_ADDRESS	0x18000000
#define	LED_OFFSET		0x003A

uint16_t 			led_register_value;

static void sp6_read(void *opaque, hwaddr offset, unsigned size)
{
	switch(offset)
	{
		case LED_OFFSET:
			if(size == 2)
			{
				printf ("sp6_read: Led read 0x%x\n",led_register_value);
			}
			else
				printf ("sp6_read: Led read wrong data size %d should be 2\n",size);
		break;
		default:
			printf ("sp6_read: Bad register offset 0x%x\n", (int)offset);
		break;
	}
}

static void sp6_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
	cJSON *root = cJSON_CreateObject();
	switch(offset)
	{
		case LED_OFFSET:
			cJSON_AddStringToObject(root,"perif","led");
			if(size == 2)
			{
				printf ("sp6_write: Led write 0x%x\n",value);
				led_register_value = (uint16_t) value;
				cJSON_AddNumberToObject(root,"value",led_register_value);
				sp6_emul_cmd_post(root);
			}
			else
				printf ("sp6_write: Led write wrong data size %d should be 2\n",size);
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
                          "reptar_sp6", SP6_BASE_ADDRESS);
    	sysbus_init_mmio(sbd, &s->iomem);

	sysbus_init_irq(sbd, &s->irq);

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
