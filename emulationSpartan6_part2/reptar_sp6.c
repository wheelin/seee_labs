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

static void sp6_read(void *opaque, hwaddr offset, unsigned size)
{
	printf("sp6 read\n");
}

static void sp6_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
	printf("sp6 write\n");
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
                          "reptar_sp6", 0x18000000);
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
