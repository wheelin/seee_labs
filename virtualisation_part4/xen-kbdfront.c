/*
 * Xen para-virtual input device
 *
 * Copyright (C) 2005 Anthony Liguori <aliguori@us.ibm.com>
 * Copyright (C) 2006-2008 Red Hat, Inc., Markus Armbruster <armbru@redhat.com>
 *
 *  Based on linux/drivers/input/mouse/sermouse.c
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

/*
 * TODO:
 *
 * Switch to grant tables together with xen-fbfront.c.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/input.h>

#include <xen-guest/evtchn.h>
#include <xen-guest/hypervisor.h>
#include <xen-guest/io/fbif.h>
#include <xen-guest/io/kbdif.h>
#include <xen-guest/xenbus.h>

/* Handling of touchscreen in DomU depends on FB dimensions */
void xenfb_get_dimensions(int *width, int *height);

#define FB_DEFAULT_WIDTH    480
#define FB_DEFAULT_HEIGHT   800

struct fb_dimensions {
	int min_x;
	int max_x;
	int min_y;
	int max_y;
};
static struct fb_dimensions fb_dim = {
	.min_x = 0,
	.max_x = FB_DEFAULT_WIDTH,
	.min_y = 0,
	.max_y = FB_DEFAULT_HEIGHT,
};


/**
 * XEN Virtual KBD device
 */
struct xenkbd_info {
	struct input_dev *kbd;
	struct input_dev *ptr;
	struct input_dev *ts;
	struct xenkbd_page *page;
	int gref;
	int irq;
	struct xenbus_device *xbdev;
	char phys[32];
};

static int xenkbd_remove(struct xenbus_device *);
static int xenkbd_connect_backend(struct xenbus_device *, struct xenkbd_info *);
static void xenkbd_disconnect_backend(struct xenkbd_info *);

/* (LCD) touchscreen input event handler
 * Converts a xenkbd_ts_event into a valid sequence of input_events for
 * clearpad-style touchscreens*/
static void tsdev_input_event(struct input_dev *dev, struct xenkbd_ts_event *event)
{
	static int touch_down = 0;

	if (event->touch) {
		/* Verify that the touch event is in the area of interest for domU */
		if ((event->abs_x >= fb_dim.min_x) && (event->abs_x <= fb_dim.max_x) &&
		    (event->abs_y >= fb_dim.min_y) && (event->abs_y <= fb_dim.max_y) ) {

			// printk("VKBD: touchscreen press @ %d:%d\n", event->abs_x, event->abs_y);

			/* button down on finger contact */
			input_report_abs(dev, ABS_MISC, 1);

			/* Report input in domU's coordinates format */
			input_report_abs(dev, ABS_X, event->abs_x);
			input_report_abs(dev, ABS_Y, event->abs_y);

			/* (LCD) we simulate a normal pressure with a standard width */
			input_report_abs(dev, ABS_PRESSURE, 50);
			input_report_abs(dev, ABS_TOOL_WIDTH, 1);
			input_report_key(dev, BTN_TOUCH, 1);
			input_sync(dev);

			touch_down = 1;
		}
	} else {
		/* Only report a "finger up" event if a "finger down" was previously signaled */
		if (touch_down) {
			input_report_abs(dev, ABS_PRESSURE, 0);
			input_report_key(dev, BTN_TOUCH, 0);
			input_sync(dev);
			touch_down = 0;
		}
	}
	// printk("struct xenkbd_ts_event: %s %d %d %d\n",
		// dev->name ,event->abs_x, event->abs_y, event->touch);
}

/*
 * Note: if you need to send out events, see xenfb_do_update() for how
 * to do that.
 */
#define BOTTOM_BUT 108
#define RIGHT_BUT 106
#define CENTER_BUT 28
#define LEFT_BUT 105

#define LED_NUM 3

extern void send_led_request(int id, int brightness);


static irqreturn_t input_handler(int rq, void *dev_id)
{
	int i = 0;
	struct xenkbd_info *info = dev_id;
	struct xenkbd_page *page = info->page;
	__u32 cons, prod;

	prod = page->in_prod;
	if (prod == page->in_cons)
		return IRQ_HANDLED;
	rmb();			/* ensure we see ring contents up to prod */
	for (cons = page->in_cons; cons != prod; cons++) {
		union xenkbd_in_event *event;
		struct input_dev *dev;
		event = &XENKBD_IN_RING_REF(page, cons);

		dev = info->ptr;
		switch (event->type) {
		case XENKBD_TYPE_MOTION:
			input_report_rel(dev, REL_X, event->motion.rel_x);
			input_report_rel(dev, REL_Y, event->motion.rel_y);
			break;
		case XENKBD_TYPE_KEY:
			dev = NULL;
			if (test_bit(event->key.keycode, info->kbd->keybit))
				dev = info->kbd;
			if (test_bit(event->key.keycode, info->ptr->keybit))
				dev = info->ptr;

			//EMG modify
			/*if (dev)
				input_report_key(dev, event->key.keycode,
						 event->key.pressed);*/
			if (dev)
			{
				//input_report_key(dev, event->key.keycode,event->key.pressed);
				if(event->key.pressed == 1)
				{
					switch (event->key.keycode) 
					{
						case RIGHT_BUT:
							send_led_request(0,1);
							break;
						case CENTER_BUT:
							send_led_request(1,1);
							break;
						case LEFT_BUT:
							send_led_request(2,1);
							break;
						case BOTTOM_BUT:
							for(i = 0; i < LED_NUM; i++)
								send_led_request(i,0);
							break;
						default:
							break;
					}
				}
			}
			else
				printk("unhandled keycode 0x%x\n",
					   event->key.keycode);
			break;
		case XENKBD_TYPE_POS:
			input_report_abs(dev, ABS_X, event->pos.abs_x);
			input_report_abs(dev, ABS_Y, event->pos.abs_y);
			break;
		case XENKBD_TYPE_TS_POS:
			dev = info->ts;
			tsdev_input_event(dev,(struct xenkbd_ts_event*) event);
			break;
		default:
			printk("%s unhandled XENKBD_TYPE %d !\n", __FUNCTION__, event->type);
			break;
		}
		if (dev)
			input_sync(dev);
	}
	mb(); /* ensure we got ring contents */
	page->in_cons = cons;

	return IRQ_HANDLED;
}

static int __devinit xenkbd_probe(struct xenbus_device *dev, const struct xenbus_device_id *id)
{
	int ret, i /*, abs*/;
	struct xenkbd_info *info;
	struct input_dev *kbd, *ptr, *ts;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		xenbus_dev_fatal(dev, -ENOMEM, "allocating info structure");
		return -ENOMEM;
	}
	dev_set_drvdata(&dev->dev, info);
	info->xbdev = dev;
	info->irq = -1;
	info->gref = -1;
	snprintf(info->phys, sizeof(info->phys), "xenbus/%s", dev->nodename);

	info->page = (void *)__get_free_page(GFP_KERNEL | __GFP_ZERO);
	if (!info->page)
		goto error_nomem;

	/* KEYBOARD */
	kbd = input_allocate_device();
	if (!kbd)
		goto error_nomem;
	kbd->name = "Xen Virtual Keyboard";
	kbd->phys = info->phys;
	kbd->id.bustype = BUS_PCI;
	kbd->id.vendor = 0x5853;
	kbd->id.product = 0xffff;

	__set_bit(EV_KEY, kbd->evbit);
	for (i = KEY_ESC; i < KEY_UNKNOWN; i++)
		__set_bit(i, kbd->keybit);
	for (i = KEY_OK; i < KEY_MAX; i++)
		__set_bit(i, kbd->keybit);

	ret = input_register_device(kbd);
	if (ret) {
		input_free_device(kbd);
		xenbus_dev_fatal(dev, ret, "input_register_device(kbd)");
		goto error;
	}

	info->kbd = kbd;


	/* TOUCHSCREEN */
	ts = input_allocate_device();
	if (!ts) {
		goto error_nomem;
	}
	ts->name = "Xen Virtual Touchscreen";
	ts->phys = info->phys;
	__set_bit(EV_SYN, ts->evbit);
	__set_bit(EV_KEY, ts->evbit);
	__set_bit(EV_ABS, ts->evbit);
	__set_bit(BTN_TOUCH, ts->keybit);

	input_set_abs_params(ts, ABS_X, 0, fb_dim.max_x, 0, 0);
	input_set_abs_params(ts, ABS_Y, 0, fb_dim.max_y, 0, 0);
	input_set_abs_params(ts, ABS_PRESSURE, 0, 0xFF, 0, 0);
	input_set_abs_params(ts, ABS_TOOL_WIDTH, 0, 0x0F, 0, 0);
	input_set_abs_params(ts, ABS_MISC, 0, 0x03, 0, 0);

	ret = input_register_device(ts);
	if (ret) {
		input_free_device(ts);
		xenbus_dev_fatal(dev, ret, "input_register_device(ts)");
		goto error;
	}

	info->ts = ts;


	/* POINTING DEVICE */
	ptr = input_allocate_device();
	if (!ptr)
		goto error_nomem;
	ptr->name = "Xen Virtual Pointer";
	ptr->phys = info->phys;
	ptr->id.bustype = BUS_PCI;
	ptr->id.vendor = 0x5853;
	ptr->id.product = 0xfffe;

	input_set_capability(ptr, EV_REL, REL_X);
	input_set_capability(ptr, EV_REL, REL_Y);

	__set_bit(EV_KEY, ptr->evbit);
	for (i = BTN_LEFT; i <= BTN_TASK; i++)
		__set_bit(i, ptr->keybit);

	ret = input_register_device(ptr);
	if (ret) {
		input_free_device(ptr);
		xenbus_dev_fatal(dev, ret, "input_register_device(ptr)");
		goto error;
	}
	info->ptr = ptr;

	ret = xenkbd_connect_backend(dev, info);
	if (ret < 0)
		goto error;

	return 0;

 error_nomem:
	ret = -ENOMEM;
	xenbus_dev_fatal(dev, ret, "allocating device memory");
 error:
	xenkbd_remove(dev);
	return ret;
}

static int xenkbd_resume(struct xenbus_device *dev)
{
	struct xenkbd_info *info = dev_get_drvdata(&dev->dev);

	xenkbd_disconnect_backend(info);
	memset(info->page, 0, PAGE_SIZE);
	return xenkbd_connect_backend(dev, info);
}

static int xenkbd_remove(struct xenbus_device *dev)
{
	struct xenkbd_info *info = dev_get_drvdata(&dev->dev);

	xenkbd_disconnect_backend(info);
	if (info->kbd)
		input_unregister_device(info->kbd);
	if (info->ptr)
		input_unregister_device(info->ptr);
	free_page((unsigned long)info->page);
	kfree(info);
	return 0;
}

/* Static useful to retrieve frontend info post-migration */
static unsigned int kbdfront_evtchn = 0;
static unsigned long kbdfront_shared_page_mfn = 0;

static int xenkbd_connect_backend(struct xenbus_device *dev, struct xenkbd_info *info)
{
	int ret, evtchn;
	struct xenbus_transaction xbt;

	ret = xenbus_alloc_evtchn(dev, &evtchn);
	if (ret)
		goto error_grant;

	kbdfront_evtchn = evtchn;

	printk("%s: bind_evtchn_to_irqhandler\n", __FUNCTION__);
	ret = bind_evtchn_to_irqhandler(evtchn, input_handler, 0, dev->devicetype, info);
	if (ret < 0) {
		xenbus_dev_fatal(dev, ret, "bind_evtchn_to_irqhandler");
		goto error_evtchan;
	}
	info->irq = ret;

 again:
	ret = xenbus_transaction_start(&xbt);
	if (ret) {
		xenbus_dev_fatal(dev, ret, "starting transaction");
		goto error_irqh;
	}

	ret = xenbus_printf(xbt, dev->nodename, "page-ref", "%lu",
			    virt_to_mfn(info->page));
	if (ret)
		goto error_xenbus;

	kbdfront_shared_page_mfn = virt_to_mfn(info->page);

	ret = xenbus_printf(xbt, dev->nodename, "page-gref", "%u", info->gref);
	if (ret)
		goto error_xenbus;

	ret = xenbus_printf(xbt, dev->nodename, "event-channel", "%u",
			    evtchn);
	if (ret)
		goto error_xenbus;

	ret = xenbus_transaction_end(xbt, 0);
	if (ret) {
		if (ret == -EAGAIN)
			goto again;
		xenbus_dev_fatal(dev, ret, "completing transaction");
		goto error_irqh;
	}

	xenbus_switch_state(dev, XenbusStateInitialised);
	return 0;

 error_xenbus:
	xenbus_transaction_end(xbt, 1);
	xenbus_dev_fatal(dev, ret, "writing xenstore");
 error_irqh:
	unbind_from_irqhandler(info->irq, info);
	info->irq = -1;
 error_evtchan:
	xenbus_free_evtchn(dev, evtchn);
 error_grant:
	info->gref = -1;
	return ret;
}

static void xenkbd_disconnect_backend(struct xenkbd_info *info)
{
	if (info->irq >= 0)
		unbind_from_irqhandler(info->irq, info);
	info->irq = -1;

	info->gref = -1;
}

static void xenkbd_backend_changed(struct xenbus_device *dev,
				   enum xenbus_state backend_state)
{
	struct xenkbd_info *info = dev_get_drvdata(&dev->dev);
	int ret, val;

	switch (backend_state) {
	case XenbusStateInitialising:
	case XenbusStateInitialised:
	case XenbusStateReconfiguring:
	case XenbusStateReconfigured:
	case XenbusStateUnknown:
	case XenbusStateClosed:
		break;

	case XenbusStateInitWait:
InitWait:
		ret = xenbus_scanf(XBT_NIL, info->xbdev->otherend, "feature-abs-pointer", "%d", &val);
		if (ret < 0)
			val = 0;
		if (val) {
			ret = xenbus_printf(XBT_NIL, info->xbdev->nodename, "request-abs-pointer", "1");
			if (ret)
				pr_warning("xenkbd: can't request abs-pointer");
		}

		xenbus_switch_state(dev, XenbusStateConnected);
		break;

	case XenbusStateConnected:
		/*
		 * Work around xenbus race condition: If backend goes
		 * through InitWait to Connected fast enough, we can
		 * get Connected twice here.
		 */
		if (dev->state != XenbusStateConnected)
			goto InitWait; /* no InitWait seen yet, fudge it */

		/* Get Framebuffer dimensions (which should have been initialised first!) */
		xenfb_get_dimensions(&fb_dim.max_x, &fb_dim.max_y);
		printk("VKDB: retrieved FB dimensions (width:%d, heigth:%d)\n",
			   fb_dim.max_x, fb_dim.max_y);
		break;

	case XenbusStateClosing:
		xenbus_frontend_closed(dev);
		break;
	}
}

static const struct xenbus_device_id xenkbd_ids[] = {
	{ "vkbd" },
	{ "" }
};

static struct xenbus_driver xenkbd_driver = {
	.name = "vkbd",
	.owner = THIS_MODULE,
	.ids = xenkbd_ids,
	.probe = xenkbd_probe,
	.remove = xenkbd_remove,
	.resume = xenkbd_resume,
	.otherend_changed = xenkbd_backend_changed,
};

static int __init xenkbd_init(void)
{
	if (!xen_domain())
		return -ENODEV;

	/* Nothing to do if running in dom0. */
	if (xen_initial_domain())
		return -ENODEV;

	return xenbus_register_frontend(&xenkbd_driver);
}

static void __exit xenkbd_cleanup(void)
{
	xenbus_unregister_driver(&xenkbd_driver);
}

/*------------------------------------------------------------------------------
    Post-migration routines
------------------------------------------------------------------------------*/
int do_domcall_get_kbd_frontend_info(void *arg)
{
	struct DOMCALL_get_kbd_frontend_info_args *args = arg;

	args->shared_page_pfn = kbdfront_shared_page_mfn;
	args->evtchn_port     = kbdfront_evtchn;

	return 0;
}

module_init(xenkbd_init);
module_exit(xenkbd_cleanup);

MODULE_DESCRIPTION("Xen virtual keyboard/pointer device frontend");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xen:vkbd");