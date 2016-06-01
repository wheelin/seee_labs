/*
 * Virtual LED interface backend for EmbeddedXEN
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

#include <linux/vmalloc.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/irqreturn.h>
#include <asm/pgtable.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/leds.h>

/* EmbeddedXEN */
#include <xen-guest/evtchn.h>
#include <xen-guest/hypercall-arm.h>
#include <xen-guest/io/ledif.h>
#include <xen-guest/xenbus.h>

#include <asm/io.h>

/***** End of Includes *****/

/***** Typedefs and structures *****/

/* Backend structure */
struct backend_info {
	struct xenbus_device	*dev;
	struct xenbus_watch		backend_watch;
};

/*
 * Structure that includes the ring, the associated grant table reference,
 * the event channel number and the IRQ number
 */
struct xenvled_back {
	domid_t				domid;
	unsigned int		handle;

	xenled_back_ring_t	ring;
	grant_ref_t			ring_ref;
	evtchn_port_t		evtchn;
	unsigned int		irq;
};

/***** End of Typedefs and structures *****/

/***** Global variables *****/

static struct xenvled_back xenvledinfo_glob;
static struct xenvled_back *xenvledinfo; /* Initialized in the probe */
uint16_t *reg;

/***** End of Global variables *****/

/***** DomU > Dom0 Request *****/

/*
 * Process a request coming from DomU
 * \param id: Identifier (feel free to do whatever you want with it; it
 *            should allow you to identify the LED that you want to drive)
 * \param brightness: Desired brightness
 */
static void receive_led_request(int id, int brightness)
{
	/* How to interact with the LED subsystem and drive the LEDs? */
	printk("%s(%d, %d)\n", __func__, id, brightness);
	
	// Set the led brightness
	if(brightness)
		*reg |= 0x0001 << id;
	else
		*reg &= ~ (0x0001 << id);
}

/*
 * Consume the incoming request-s in the ring and call the proper processing
 * function
 */
static void consume_ledback_request(void)
{
	RING_IDX i, rp;
	struct xenled_request *ring_rq;

again:
	rp = xenvledinfo->ring.sring->req_prod;

	rmb(); /* Ensure we see queued responses up to 'rp'. */

	for (i = xenvledinfo->ring.req_cons; i != rp; i++) {
		ring_rq = RING_GET_REQUEST(&xenvledinfo->ring, i);

		/* Call the processing function */
		receive_led_request(ring_rq->id, ring_rq->brightness);
	}

	xenvledinfo->ring.req_cons = i;
	xenvledinfo->ring.sring->req_cons = i;

	if (i != xenvledinfo->ring.rsp_prod_pvt) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_REQUESTS(&xenvledinfo->ring, more_to_do);
		if (more_to_do) {
			goto again;
		}
	} else {
		xenvledinfo->ring.sring->req_event = i + 1;
	}
}

/*
 * Function called when an incoming request IRQ is raised.
 */
static irqreturn_t ledback_interrupt(int irq, void *dev_id)
{
	/* Consume the incoming request-s */
	consume_ledback_request();

	return IRQ_HANDLED;
}

/***** End of DomU > Dom0 request *****/

/***** Probe, ring setup and state machine *****/

/*
 * Ring setup
 */
static int setup_sring(void)
{
	int err;
	struct evtchn_alloc_unbound alloc_unbound;
	xenled_sring_t *sring;

	/* Allocate a page */
	sring = (xenled_sring_t *)__get_free_page(GFP_KERNEL);

	if (!sring) {
		printk(KERN_ERR "Error: Could not allocate shared ring");
		return -ENOMEM;
	}

	/* Initialize the ring */
	SHARED_RING_INIT(sring);

	/* Initialize the ring at the backend's side */
	BACK_RING_INIT(&xenvledinfo->ring, sring, PAGE_SIZE);

	/* Share the ring */
	err = gnttab_grant_foreign_access(1 /* DomU */,
										virt_to_mfn(xenvledinfo->ring.sring),
										0);
	if (err < 0) {
		printk(KERN_ERR "Error: Could not grant access to ring page\n");
		free_page((unsigned long)xenvledinfo->ring.sring);
		xenvledinfo->ring.sring = NULL;
		goto fail;
	}
	xenvledinfo->ring_ref = err;

	/* Allocate the event channel */
	alloc_unbound.dom = DOMID_SELF;
	alloc_unbound.remote_dom = 1 /* DomU */;

	err = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound, &alloc_unbound);
	if (err) {
		printk(KERN_ERR "Error: Could not allocate event channel\n");
		goto fail;
	}
	else
		xenvledinfo->evtchn = alloc_unbound.port;

	/* Bind to the irqhandler */
	err = bind_evtchn_to_irqhandler(xenvledinfo->evtchn,
									ledback_interrupt,
									0,
									"vled",
									xenvledinfo);
	if (err <= 0) {
		printk(KERN_ERR "Error: bind_evtchn_to_irqhandler failed\n");
		goto fail;
	}
	xenvledinfo->irq = err;

	/* Save the information on xenbus */
	xenbus_printf(XBT_NIL, "backend/vled", "ring-ref", "%u",
		(unsigned int) xenvledinfo->ring_ref);
	xenbus_printf(XBT_NIL, "backend/vled", "event-channel", "%u",
		(unsigned int) xenvledinfo->evtchn);

	return 0;

fail:
	return err;

}

/*
 * Probe function
 */
static int ledback_probe(struct xenbus_device *dev, const struct xenbus_device_id *id)
{
	struct backend_info *be = kzalloc(sizeof(struct backend_info), GFP_KERNEL);
	int result;

	/* Backend structure */
	if (!be) {
		xenbus_dev_fatal(dev, -ENOMEM, "allocating backend structure");
		return -ENOMEM;
	}
	be->dev = dev;
	dev_set_drvdata(&dev->dev, be);

	printk(KERN_INFO "EmbeddedXen LED driver: Backend side of the Force\n");

	/* Ring setup */
	result = setup_sring();
	if (result)
		goto fail;

	// Remap the /sys/class/leds to access it
	reg = ioremap(0x18000000+0x3a,2);

	/* OK, now the backend can go to the Connected state */
	xenbus_switch_state(dev, XenbusStateConnected);

	return result;

fail:
	printk("failed");
	return result;
}

/*
 * Function called when the frontend's state changes
 */
static void frontend_changed(struct xenbus_device *dev, enum xenbus_state frontend_state)
{
	printk("%s", xenbus_strstate(frontend_state));

	switch (frontend_state) {
	case XenbusStateInitialising:
		xenbus_switch_state(dev, XenbusStateConnected); /* actually, reconnected for further actions */
		break;

	case XenbusStateInitialised:
	case XenbusStateConnected:
		printk("->LED Frontend connected, all right.\n");
		break;

	case XenbusStateClosing:
		printk("EmbeddedXEN: LED Frontend now closing...\n");

		xenbus_switch_state(dev, XenbusStateClosing);
		break;

	case XenbusStateClosed:
		/* Prepare to empty all buffers */
		SHARED_RING_INIT((&xenvledinfo->ring)->sring);
		BACK_RING_INIT(&xenvledinfo->ring, (&xenvledinfo->ring)->sring, PAGE_SIZE);

		xenbus_switch_state(dev, XenbusStateClosed);

		break;

	/* Not used here */
	case XenbusStateSuspending:
	case XenbusStateSuspended:
	case XenbusStateResuming:
	case XenbusStateResumed:
		break;

	case XenbusStateUnknown:
		break;

	default:
		xenbus_dev_fatal(dev, -EINVAL, "saw state %d at frontend", frontend_state);
		break;
	}
}

/***** End of Probe, ring setup and state machine *****/

/***** Essential structs and init *****/

static const struct xenbus_device_id ledback_ids[] = {
		{ "vled" },
		{ "" }
};

static struct xenbus_driver ledback_driver = {
		.name = "vled",
		.owner = THIS_MODULE,
		.ids = ledback_ids,
		.probe = ledback_probe,
		.otherend_changed = frontend_changed,
};

static int ledback_init(void)
{
	xenvledinfo = &xenvledinfo_glob;

	return xenbus_register_backend(&ledback_driver);
}


module_init(ledback_init);

/***** End of Essential structs and init *****/

MODULE_DESCRIPTION("EmbeddedXEN virtual LED device backend");
MODULE_LICENSE("GPL");