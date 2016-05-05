/*******************************************************************
 * reptar_sp6_leds.c
 *
 * Author: Romain Bornet (RBO)
 * Copyright (c) 2013 HEIG-VD, REDS Institute
 *******************************************************************/

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/io.h>
#include "reptar_sp6.h"

static DEFINE_SPINLOCK(reg_lock);

static struct platform_device *reptar_sp6_leds[SP6_NUM_LEDS];

struct reptar_sp6_led {
	struct led_classdev cdev;
	struct reptar_sp6_led_platdata *pdata;
	uint16_t *reg;
};

static inline struct reptar_sp6_led *pdev_to_sp6_led(struct platform_device *dev)
{
	return platform_get_drvdata(dev);
}

static inline struct reptar_sp6_led *leddev_to_sp6_led(struct led_classdev *led_cdev)
{
	return container_of(led_cdev, struct reptar_sp6_led, cdev);
}

static void reptar_sp6_led_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	struct reptar_sp6_led *led = leddev_to_sp6_led(led_cdev);
	struct reptar_sp6_led_platdata *pd = led->pdata;

	/* Protect access to shared register */
	spin_lock(&reg_lock);

	/* to be completed */
	if(value)
		*led->reg |= 0x0001 << (pd->bit);
	else
		*led->reg &= ~ (0x0001 << (pd->bit));
	spin_unlock(&reg_lock);

}

static int reptar_sp6_led_remove(struct platform_device *dev)
{
	struct reptar_sp6_led *led = pdev_to_sp6_led(dev);

	kfree(led);

	return 0;
}

static int reptar_sp6_led_probe(struct platform_device *pdev)
{
	struct reptar_sp6_led_platdata *pdata = pdev->dev.platform_data;
	struct reptar_sp6_led *led;
	struct platform_device *fpga_pdev;
	struct resource *fpga_resource;
	int ret;

	led = kzalloc(sizeof(struct reptar_sp6_led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&pdev->dev, "No memory for device\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, led);

	led->cdev.brightness_set = reptar_sp6_led_set;
	led->cdev.name = pdata->name;

	led->pdata = pdata;

	fpga_pdev = to_platform_device(pdev->dev.parent);
	if (!fpga_pdev) {
		dev_err(&pdev->dev, "can't get device parent\n");
		return -ENOENT;
	}

	fpga_resource = platform_get_resource(fpga_pdev, IORESOURCE_MEM, 0);
	if (!fpga_resource) {
		dev_err(&pdev->dev, "can't get parent device's resources\n");
		return -ENOENT;
	}

	/* Map the LED register */

	led->reg = ioremap(fpga_resource->start+pdata->reg_offset,2);

	/* Register our new led device into led class */

	led_classdev_register(&pdev->dev.parent, &led->cdev);

	return 0;
}

static struct platform_driver reptar_sp6_led_driver = {
	.probe		= reptar_sp6_led_probe,
	.remove		= reptar_sp6_led_remove,
	.driver		= {
		.name		= "reptar_sp6_led",
		.owner		= THIS_MODULE,
	},
};

/* static int __init reptar_sp6_leds_init(void) */
int __init reptar_sp6_leds_init(struct platform_device *parent_fpga)
{
	int i;

	for (i = 0; i < SP6_NUM_LEDS; i++) {

		reptar_sp6_leds[i] = platform_device_alloc("reptar_sp6_led", i);
		platform_device_add_data(reptar_sp6_leds[i], &reptar_sp6_leds_pdata[i], sizeof(struct reptar_sp6_led_platdata));

		/* Set link to parent device */
		reptar_sp6_leds[i]->dev.parent = &(parent_fpga->dev);

		platform_device_add(reptar_sp6_leds[i]);
	}
	return platform_driver_register(&reptar_sp6_led_driver);
}

void __exit reptar_sp6_leds_exit(void)
{
	int i;

	for (i = 0; i < SP6_NUM_LEDS; i++) {
		platform_device_unregister(reptar_sp6_leds[i]);
	}

	platform_driver_unregister(&reptar_sp6_led_driver);
}
