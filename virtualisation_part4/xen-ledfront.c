/*
 * Virtual LED interface frontend for EmbeddedXEN
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SEEE 2016 - DEB
 */

/***** Includes *****/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/leds.h>
#include <linux/slab.h>

/* EmbeddedXEN */
#include <xen-guest/evtchn.h>
#include <xen-guest/hypervisor.h>
#include <xen-guest/io/ledif.h>
#include <xen-guest/xenbus.h>

/***** End of Includes *****/

/***** Typedefs and structures *****/

/*
 * Structure that includes the ring, the associated grant table reference,
 * the event channel number and the IRQ number
 */
struct xenvled_front {
	domid_t				domid;
	unsigned int		handle;

	xenled_front_ring_t	ring;
	grant_ref_t			ring_ref;
	evtchn_port_t		evtchn;
	unsigned int		irq;
};

/*
 * Structure used to interact with the LED subsystem. It includes a cdev
 * structure associated to a sysfs entry
 */
struct xenvled_data {
	char				*name;
	int					id;
	struct led_classdev	cdev;
};

/***** End of Typedefs and structures *****/

/***** Global variables *****/

static struct xenvled_front xenvledinfo_glob;
static struct xenvled_front *xenvledinfo; /* Initialized in the probe */

/***** End of Global variables *****/

/***** Dom0 > DomU Response *****/

/*
 * Function called when an incoming response IRQ is raised. Consume the incoming
 * response in the ring and call the proper processing function; please note
 * that there is nothing particular to do here (for the lab)
 */
static irqreturn_t ledfront_interrupt(int irq, void *dev_id)
{
	RING_IDX i, rp;
	struct xenled_response *ring_rsp;

again:
	rp = xenvledinfo->ring.sring->rsp_prod;
	rmb(); /* Ensure we see queued responses up to 'rp'. */

	for (i = xenvledinfo->ring.rsp_cons; i != rp; i++) {
		ring_rsp = RING_GET_RESPONSE(&xenvledinfo->ring, i);

		/* Not necessary to process the response, no particular action here */
	}
	xenvledinfo->ring.rsp_cons = i;

	if (i != xenvledinfo->ring.req_prod_pvt) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_RESPONSES(&xenvledinfo->ring, more_to_do);
		if (more_to_do) {
			goto again;
		}
	}
	else {
		xenvledinfo->ring.sring->rsp_event = i + 1;
	}

	return IRQ_HANDLED;
}

/***** End of Dom0 > DomU Response *****/

/***** DomU > Dom0 Request *****/

/*
 * Send a request to Dom0
 * \param id: Identifier (feel free to do whatever you want with it; it
 *            should allow you to identify the LED that you want to drive)
 * \param brightness: Desired brightness
 */
static void send_led_request(int id, int brightness)
{
	struct xenled_request *ring_req;

	/* Fill out a communications ring structure. */
	ring_req = RING_GET_REQUEST(&xenvledinfo->ring, xenvledinfo->ring.req_prod_pvt);
	xenvledinfo->ring.req_prod_pvt++;

	ring_req->id = id;
	ring_req->brightness = brightness;

	RING_PUSH_REQUESTS(&xenvledinfo->ring);

	notify_remote_via_irq(xenvledinfo->irq);
}

/*
 * Add your functions that will interact with the LED subsystem (creation of a LED
 * entry in the sysfs, brightness_set callback...
 */
static void backend_led_set(struct led_classdev *led_cdev, enum led_brightness value);

struct backend_led {
	struct led_classdev cdev;
	uint8_t id;
};

struct backend_led back_leds[] = {
	{
		.cdev.name		= "led0",
		.cdev.brightness_set 	= backend_led_set,
		.id			= 0
	},
	{
		.cdev.name		= "led1",
		.cdev.brightness_set 	= backend_led_set,
		.id			= 1
	},
	{
		.cdev.name		= "led2",
		.cdev.brightness_set 	= backend_led_set,
		.id			= 2
	},
	{
		.cdev.name		= "led3",
		.cdev.brightness_set 	= backend_led_set,
		.id			= 3
	},
	{
		.cdev.name		= "led4",
		.cdev.brightness_set 	= backend_led_set,
		.id			= 4
	},
	{
		.cdev.name		= "led5",
		.cdev.brightness_set 	= backend_led_set,
		.id			= 5
	},
};
static void backend_led_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	struct backend_led * led = container_of(led_cdev, struct backend_led, cdev);
	send_led_request(led->id,value);
}

/***** End of DomU > Dom0 Request *****/

/***** Probe, ring setup and state machine *****/

/*
 * Map the ring page allocated by the backend
 */
static int map_backend_page(xenled_sring_t **sring)
{
	struct gnttab_map_grant_ref op;
	struct vm_struct *area = alloc_vm_area(PAGE_SIZE, NULL);

	if (!area)
		return -ENOMEM;

	gnttab_set_map_op(&op, (unsigned long)area->addr, GNTMAP_host_map, xenvledinfo->ring_ref, 0);

	if (HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1))
		BUG();

	if (op.status) {
		printk(KERN_ERR "Error : Grant table operation failure !\n");
		return op.status;
	}

	*sring = (xenled_sring_t *)area->addr;
	xenvledinfo->handle = op.handle;

	return 0;
}

/*
 * Ring setup
 */
static int setup_sring(void)
{
	int err;
	xenled_sring_t *sring = NULL;

	/* Retrieve the information on xenbus */
	xenbus_scanf(XBT_NIL, "backend/vled", "ring-ref", "%u", &xenvledinfo->ring_ref);
	xenbus_scanf(XBT_NIL, "backend/vled", "event-channel", "%u", &xenvledinfo->evtchn);

	/* Map the page */
	err = map_backend_page(&sring);
	if (err)
		return err;

	/* Initialize the ring at the frontend's side */
	FRONT_RING_INIT(&xenvledinfo->ring, sring, PAGE_SIZE);

	/* Binding the irqhandler to the eventchannel */
	err = bind_interdomain_evtchn_to_irqhandler(0 /* from Dom0 */,
							xenvledinfo->evtchn,
							ledfront_interrupt,
							0,
							"vled",
							xenvledinfo);

	if (err <= 0) {
		printk(KERN_ERR "Error: bind_evtchn_to_irqhandler failed");
		goto fail;
	}
	xenvledinfo->irq = err;

	return 0;
fail:

	return err;

}

/*
 * Probe function
 */
static int ledfront_probe(struct xenbus_device *dev, const struct xenbus_device_id *id)
{
	int result;

	printk(KERN_INFO "EmbeddedXen LED driver: Frontend side of the Force\n");

	/* Ring setup */
	result = setup_sring();
	if (result)
		return result;

	/* How to register the sysfs entries for the LED subsystem? */

	/* Register the sysfs entries for the LED subsystem*/
	led_classdev_register(&dev->dev, &back_leds[0].cdev);
	led_classdev_register(&dev->dev, &back_leds[1].cdev);
	led_classdev_register(&dev->dev, &back_leds[2].cdev);
	led_classdev_register(&dev->dev, &back_leds[3].cdev);
	led_classdev_register(&dev->dev, &back_leds[4].cdev);
	led_classdev_register(&dev->dev, &back_leds[5].cdev);

	/* OK, now the frontend can go to the Connected state */
	xenbus_switch_state(dev, XenbusStateConnected);

	return result;
}

/*
 * Function called when the backend's state changes
 */
static void backend_changed(struct xenbus_device *dev, enum xenbus_state backend_state)
{
	printk("%s", xenbus_strstate(backend_state));

	switch (backend_state) {
	case XenbusStateInitialising:
	case XenbusStateInitWait:
	case XenbusStateInitialised:
	case XenbusStateConnected:
	case XenbusStateReconfiguring:
	case XenbusStateReconfigured:
	case XenbusStateUnknown:
	case XenbusStateClosed:
		break;

	case XenbusStateClosing:
		xenbus_frontend_closed(dev);
		break;

	/* Not used here */
	case XenbusStateSuspending:
	case XenbusStateSuspended:
	case XenbusStateResuming:
	case XenbusStateResumed:
		break;

	default:
		xenbus_dev_fatal(dev, -EINVAL, "backend_state %d", backend_state);
		break;
	}
}

/***** End of Probe, ring setup and state machine *****/

/***** Essential structs and init *****/

static const struct xenbus_device_id ledfront_ids[] = {
	{ "vled" },
	{ "" }
};

static struct xenbus_driver ledfront_driver = {
	.name = "vled",
	.owner = THIS_MODULE,
	.ids = ledfront_ids,
	.probe = ledfront_probe,
	.otherend_changed = backend_changed,
};

static int __init ledfront_init(void)
{
	int ret;

	if (!xen_domain())
		return -ENODEV;

	xenvledinfo = &xenvledinfo_glob;

	ret = xenbus_register_frontend(&ledfront_driver);

	return ret;
}

module_init(ledfront_init);

/***** End of Essential structs and init *****/

MODULE_DESCRIPTION("EmbeddedXEN virtual LED device frontend");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xen:vled");