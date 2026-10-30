// SPDX-License-Identifier: GPL-2.0+
/*
 * f_hidmouse.c -- USB HID mouse function driver (skeleton)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/usb/composite.h>

#define HIDMOUSE_REPORT_LEN	3

/*-------------------------------------------------------------------------*/
/*                          static descriptors                             */


static const u8 hidmouse_report_desc[] = {
	0x05, 0x01,	/* Usage Page (Generic Desktop) */
	0x09, 0x02,	/* Usage (Mouse) */
	0xa1, 0x01,	/* Collection (Application) */
	0x09, 0x01,	/*   Usage (Pointer) */
	0xa1, 0x00,	/*   Collection (Physical) */
	0x05, 0x09,	/*     Usage Page (Button) */
	0x19, 0x01,	/*     Usage Minimum (1) */
	0x29, 0x03,	/*     Usage Maximum (3) */
	0x15, 0x00,	/*     Logical Minimum (0) */
	0x25, 0x01,	/*     Logical Maximum (1) */
	0x95, 0x03,	/*     Report Count (3) */
	0x75, 0x01,	/*     Report Size (1) */
	0x81, 0x02,	/*     Input (Data,Var,Abs): 3 buttons */
	0x95, 0x01,	/*     Report Count (1) */
	0x75, 0x05,	/*     Report Size (5) */
	0x81, 0x01,	/*     Input (Const): padding */
	0x05, 0x01,	/*     Usage Page (Generic Desktop) */
	0x09, 0x30,	/*     Usage (X) */
	0x09, 0x31,	/*     Usage (Y) */
	0x15, 0x81,	/*     Logical Minimum (-127) */
	0x25, 0x7f,	/*     Logical Maximum (127) */
	0x75, 0x08,	/*     Report Size (8) */
	0x95, 0x02,	/*     Report Count (2) */
	0x81, 0x06,	/*     Input (Data,Var,Rel): dx, dy */
	0xc0,		/*   End Collection */
	0xc0,		/* End Collection */
};

static struct usb_interface_descriptor hidmouse_interface_desc = {
	.bLength		    = sizeof(hidmouse_interface_desc),
	.bDescriptorType	= USB_DT_INTERFACE,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= USB_CLASS_HID,
	.bInterfaceSubClass	= USB_INTERFACE_SUBCLASS_BOOT,
	.bInterfaceProtocol	= USB_INTERFACE_PROTOCOL_MOUSE,
};

static struct hid_descriptor hidmouse_hid_desc = {
	.bLength			        = sizeof(hidmouse_hid_desc),
	.bDescriptorType		    = HID_DT_HID,
	.bcdHID				        = cpu_to_le16(0x0111),
	.bCountryCode			    = 0x00,
	.bNumDescriptors		    = 1,
	.desc[0].bDescriptorType	= HID_DT_REPORT,
	.desc[0].wDescriptorLength	= cpu_to_le16(sizeof(hidmouse_report_desc)),
};

/* full-speed endpoint descriptor */
static struct usb_endpoint_descriptor hidmouse_fs_in_ep_desc = {
	.bLength		    = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize		= cpu_to_le16(HIDMOUSE_REPORT_LEN),
	.bInterval		    = 10,		/* 10 ms */
};

/* high-speed endpoint descriptor */
static struct usb_endpoint_descriptor hidmouse_hs_in_ep_desc = {
	.bLength		    = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize		= cpu_to_le16(HIDMOUSE_REPORT_LEN),
	.bInterval		    = 4,
};

/* super-speed endpoint descriptor */
static struct usb_endpoint_descriptor hidmouse_ss_in_ep_desc = {
	.bLength		    = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize		= cpu_to_le16(HIDMOUSE_REPORT_LEN),
	.bInterval		    = 4,
};

static struct usb_ss_ep_comp_descriptor hidmouse_ss_in_comp_desc = {
	.bLength		= sizeof(hidmouse_ss_in_comp_desc),
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	.wBytesPerInterval	= cpu_to_le16(HIDMOUSE_REPORT_LEN),
};

static struct usb_descriptor_header *hidmouse_fs_descriptors[] = {
	(struct usb_descriptor_header *)&hidmouse_interface_desc,
	(struct usb_descriptor_header *)&hidmouse_hid_desc,
	(struct usb_descriptor_header *)&hidmouse_fs_in_ep_desc,
	NULL,
};

static struct usb_descriptor_header *hidmouse_hs_descriptors[] = {
	(struct usb_descriptor_header *)&hidmouse_interface_desc,
	(struct usb_descriptor_header *)&hidmouse_hid_desc,
	(struct usb_descriptor_header *)&hidmouse_hs_in_ep_desc,
	NULL,
};

static struct usb_descriptor_header *hidmouse_ss_descriptors[] = {
	(struct usb_descriptor_header *)&hidmouse_interface_desc,
	(struct usb_descriptor_header *)&hidmouse_hid_desc,
	(struct usb_descriptor_header *)&hidmouse_ss_in_ep_desc,
	(struct usb_descriptor_header *)&hidmouse_ss_in_comp_desc,
	NULL,
};

/*-------------------------------------------------------------------------*/
/*                         HID mouse gadget struct                         */

struct f_hidmouse {
	struct usb_function		func;
	struct usb_ep			*in_ep;

	u8				protocol;	/* HID_{BOOT,REPORT}_PROTOCOL */
	u8				idle;		/* SET_IDLE duration, 4 ms units */

	/* interrupt IN data path */
	spinlock_t			lock;		/* protects below */
	struct usb_request		*req;		/* single report in flight */
	bool				enabled;	/* in_ep is up, may queue */
	bool				busy;		/* req owned by the UDC */
	int				dx, dy;		/* motion not yet reported */
	u8				buttons;	/* button bitmap to report */

	/* touch source, registered while bound */
	struct input_handler		handler;
};

/* one per matched touchscreen; state touched only from its event callback */
struct hidmouse_touch {
	struct input_handle		handle;
	int				slot;		/* current ABS_MT_SLOT */
	int				x, y;		/* latest slot 0 position */
	int				last_x, last_y;	/* position already turned into motion */
	bool				down;		/* slot 0 contact active */
	bool				have_last;	/* last_x/last_y valid */
};

static inline struct f_hidmouse *func_to_hidmouse(struct usb_function *f)
{
	return container_of(f, struct f_hidmouse, func);
}

/*-------------------------------------------------------------------------*/
/*                         interrupt IN data path                          */

/*
 * Move pending motion into req->buf (the RAM the UDC will DMA from) and hand
 * req to the UDC. The UDC answers the host's next IN token with it; with
 * nothing queued the controller NAKs on its own.
 * Caller holds hidmouse->lock, and req must not be in flight.
 */
static void hidmouse_queue_report_locked(struct f_hidmouse *hidmouse)
{
	u8	*report = hidmouse->req->buf;
	int	dx = clamp(hidmouse->dx, -127, 127);
	int	dy = clamp(hidmouse->dy, -127, 127);
	int	status;

	report[0] = hidmouse->buttons;
	report[1] = (s8)dx;
	report[2] = (s8)dy;
	hidmouse->req->length = HIDMOUSE_REPORT_LEN;

	/* keep what did not fit in one report for the next one */
	hidmouse->dx -= dx;
	hidmouse->dy -= dy;

	hidmouse->busy = true;
	status = usb_ep_queue(hidmouse->in_ep, hidmouse->req, GFP_ATOMIC);
	if (status < 0) {
		hidmouse->busy = false;
		ERROR(hidmouse->func.config->cdev,
		      "usb_ep_queue error on int endpoint %d\n", status);
	}
}

/* queue a report if there is something to say and req is free */
static void hidmouse_kick_locked(struct f_hidmouse *hidmouse)
{
	if (!hidmouse->enabled || hidmouse->busy || !hidmouse->req)
		return;
	if (!hidmouse->dx && !hidmouse->dy)
		return;
	hidmouse_queue_report_locked(hidmouse);
}

/* UDC is done with req: report sent, or flushed by disable */
static void hidmouse_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_hidmouse	*hidmouse = req->context;
	unsigned long		flags;

	spin_lock_irqsave(&hidmouse->lock, flags);
	hidmouse->busy = false;

	switch (req->status) {
	case 0:
		/* send motion that piled up while this report was in flight */
		hidmouse_kick_locked(hidmouse);
		break;
	case -ECONNRESET:	/* dequeued */
	case -ESHUTDOWN:	/* endpoint disabled / disconnect */
		break;
	default:
		ERROR(hidmouse->func.config->cdev,
		      "IN request failed %d\n", req->status);
		hidmouse_kick_locked(hidmouse);
		break;
	}
	spin_unlock_irqrestore(&hidmouse->lock, flags);
}

/*-------------------------------------------------------------------------*/
/*                         touchscreen input source                        */

/*
 * Runs in the touch driver's context with dev->event_lock held and IRQs off.
 * Only slot 0 (first finger) drives the pointer: its absolute position is
 * turned into relative motion once per frame (SYN_REPORT).
 */
static void hidmouse_input_event(struct input_handle *handle,
				 unsigned int type, unsigned int code, int value)
{
	struct hidmouse_touch	*touch = container_of(handle,
						struct hidmouse_touch, handle);
	struct f_hidmouse	*hidmouse = handle->private;
	unsigned long		flags;

	switch (type) {
	case EV_ABS:
		switch (code) {
		case ABS_MT_SLOT:
			touch->slot = value;
			break;
		case ABS_MT_TRACKING_ID:
			if (touch->slot != 0)
				break;
			/* -1: finger lifted; otherwise a new contact starts */
			touch->down = value >= 0;
			touch->have_last = false;
			break;
		case ABS_MT_POSITION_X:
			if (touch->slot == 0)
				touch->x = value;
			break;
		case ABS_MT_POSITION_Y:
			if (touch->slot == 0)
				touch->y = value;
			break;
		}
		break;

	case EV_KEY:
		/* TODO: map keys to buttons; BTN_TOUCH only tracks contact */
		if (code == BTN_TOUCH && !value) {
			touch->down = false;
			touch->have_last = false;
		}
		break;

	case EV_SYN:
		if (code != SYN_REPORT || !touch->down)
			break;

		/* first frame of a contact only sets the origin */
		if (!touch->have_last) {
			touch->last_x = touch->x;
			touch->last_y = touch->y;
			touch->have_last = true;
			break;
		}

		spin_lock_irqsave(&hidmouse->lock, flags);
		hidmouse->dx += touch->x - touch->last_x;
		hidmouse->dy += touch->y - touch->last_y;
		hidmouse->buttons = 0;	/* default: no button pressed */
		hidmouse_kick_locked(hidmouse);
		spin_unlock_irqrestore(&hidmouse->lock, flags);

		touch->last_x = touch->x;
		touch->last_y = touch->y;
		break;
	}
}

static int hidmouse_input_connect(struct input_handler *handler,
				  struct input_dev *dev,
				  const struct input_device_id *id)
{
	struct f_hidmouse	*hidmouse = container_of(handler,
						struct f_hidmouse, handler);
	struct hidmouse_touch	*touch;
	int			error;

	touch = kzalloc(sizeof(*touch), GFP_KERNEL);
	if (!touch)
		return -ENOMEM;

	touch->handle.dev = dev;
	touch->handle.handler = handler;
	touch->handle.name = "hidmouse";
	touch->handle.private = hidmouse;

	error = input_register_handle(&touch->handle);
	if (error)
		goto err_free;

	/* shared, not grabbed: Android keeps receiving the touches too */
	error = input_open_device(&touch->handle);
	if (error)
		goto err_unregister;

	pr_info("hidmouse: using %s as pointer source\n", dev->name);
	return 0;

err_unregister:
	input_unregister_handle(&touch->handle);
err_free:
	kfree(touch);
	return error;
}

static void hidmouse_input_disconnect(struct input_handle *handle)
{
	struct hidmouse_touch	*touch = container_of(handle,
						struct hidmouse_touch, handle);

	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(touch);
}

/* multitouch screens: report EV_ABS with ABS_MT_POSITION_X and _Y */
static const struct input_device_id hidmouse_input_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
				BIT_MASK(ABS_MT_POSITION_X) |
				BIT_MASK(ABS_MT_POSITION_Y) },
	},
	{ },	/* terminating entry */
};

/*-------------------------------------------------------------------------*/
/*                           usb_function ops                              */

static int hidmouse_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_hidmouse	*hidmouse = func_to_hidmouse(f);
	struct usb_ep		*ep;
	int			status;

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		return status;
	hidmouse_interface_desc.bInterfaceNumber = status;

	/* allocate instance-specific endpoints, autoconfig patches address */
	ep = usb_ep_autoconfig(c->cdev->gadget, &hidmouse_fs_in_ep_desc);
	if (!ep)
		return -ENODEV;
	hidmouse->in_ep = ep;

	/* same physical endpoint at every speed: copy address (dir | number) */
	hidmouse_hs_in_ep_desc.bEndpointAddress =
		hidmouse_fs_in_ep_desc.bEndpointAddress;
	hidmouse_ss_in_ep_desc.bEndpointAddress =
		hidmouse_fs_in_ep_desc.bEndpointAddress;

	/* SuperSpeedPlus reuses the SS set */
	status = usb_assign_descriptors(f, hidmouse_fs_descriptors,
					hidmouse_hs_descriptors,
					hidmouse_ss_descriptors,
					hidmouse_ss_descriptors);
	if (status)
		return status;

	/* connect() runs now for touchscreens already registered */
	hidmouse->handler.event		= hidmouse_input_event;
	hidmouse->handler.connect	= hidmouse_input_connect;
	hidmouse->handler.disconnect	= hidmouse_input_disconnect;
	hidmouse->handler.name		= "hidmouse";
	hidmouse->handler.id_table	= hidmouse_input_ids;
	status = input_register_handler(&hidmouse->handler);
	if (status) {
		usb_free_all_descriptors(f);
		return status;
	}

	return 0;
}

static void hidmouse_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_hidmouse	*hidmouse = func_to_hidmouse(f);

	/* disconnect() every touchscreen: no more events after this */
	input_unregister_handler(&hidmouse->handler);

	/* claimed endpoints are released by usb_ep_autoconfig_reset() */
	hidmouse->in_ep = NULL;

	usb_free_all_descriptors(f);
}

/* called on SET_CONFIGURATION / SET_INTERFACE: (re)start the IN endpoint */
static int hidmouse_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct usb_composite_dev	*cdev = f->config->cdev;
	struct f_hidmouse		*hidmouse = func_to_hidmouse(f);
	struct usb_request		*req;
	unsigned long			flags;
	int				status;

	if (alt != 0)
		return -EINVAL;

	/* restart endpoint: stop re-queueing, flush the request in flight */
	spin_lock_irqsave(&hidmouse->lock, flags);
	hidmouse->enabled = false;
	spin_unlock_irqrestore(&hidmouse->lock, flags);
	usb_ep_disable(hidmouse->in_ep);

	/* pick descriptor for the current link speed into in_ep->desc */
	status = config_ep_by_speed(cdev->gadget, f, hidmouse->in_ep);
	if (status) {
		ERROR(cdev, "config_ep_by_speed FAILED!\n");
		return status;
	}

	status = usb_ep_enable(hidmouse->in_ep);
	if (status < 0) {
		ERROR(cdev, "Enable IN endpoint FAILED!\n");
		return status;
	}
	hidmouse->in_ep->driver_data = hidmouse;

	/* one request is enough: a mouse has a single report in flight */
	if (!hidmouse->req) {
		/* atomic: set_alt runs under the composite spinlock */
		req = usb_ep_alloc_request(hidmouse->in_ep, GFP_ATOMIC);
		if (!req)
			goto fail_disable;
		req->buf = kzalloc(HIDMOUSE_REPORT_LEN, GFP_ATOMIC);
		if (!req->buf) {
			usb_ep_free_request(hidmouse->in_ep, req);
			goto fail_disable;
		}
		req->complete = hidmouse_in_complete;
		req->context = hidmouse;
		hidmouse->req = req;
	}

	/* HID 1.11 7.2.6: device returns to report protocol on configure */
	hidmouse->protocol = HID_REPORT_PROTOCOL;
	hidmouse->idle = 0;

	/* start clean; reports are queued only when touch motion arrives */
	spin_lock_irqsave(&hidmouse->lock, flags);
	hidmouse->dx = 0;
	hidmouse->dy = 0;
	hidmouse->buttons = 0;
	hidmouse->busy = false;
	hidmouse->enabled = true;
	spin_unlock_irqrestore(&hidmouse->lock, flags);

	return 0;

fail_disable:
	usb_ep_disable(hidmouse->in_ep);
	return -ENOMEM;
}

/* called on disconnect / reset / new configuration */
static void hidmouse_disable(struct usb_function *f)
{
	struct f_hidmouse	*hidmouse = func_to_hidmouse(f);
	unsigned long		flags;

	spin_lock_irqsave(&hidmouse->lock, flags);
	hidmouse->enabled = false;
	spin_unlock_irqrestore(&hidmouse->lock, flags);

	/* gives back the queued request with -ESHUTDOWN; must not hold lock */
	usb_ep_disable(hidmouse->in_ep);

	spin_lock_irqsave(&hidmouse->lock, flags);
	hidmouse->busy = false;
	spin_unlock_irqrestore(&hidmouse->lock, flags);

	if (hidmouse->req) {
		kfree(hidmouse->req->buf);
		usb_ep_free_request(hidmouse->in_ep, hidmouse->req);
		hidmouse->req = NULL;
	}
}

/* class + interface-directed standard requests on ep0 */
static int hidmouse_setup(struct usb_function *f,
			  const struct usb_ctrlrequest *ctrl)
{
	struct f_hidmouse		*hidmouse = func_to_hidmouse(f);
	struct usb_composite_dev	*cdev = f->config->cdev;
	struct usb_request		*req = cdev->req;
	u16				value, length;
	int				status;

	value	= le16_to_cpu(ctrl->wValue);
	length	= le16_to_cpu(ctrl->wLength);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE) << 8
		  | USB_REQ_GET_DESCRIPTOR):
		switch (value >> 8) {
		case HID_DT_HID:
			length = min_t(u16, length, hidmouse_hid_desc.bLength);
			memcpy(req->buf, &hidmouse_hid_desc, length);
			goto respond;
		case HID_DT_REPORT:
			length = min_t(u16, length,
				       sizeof(hidmouse_report_desc));
			memcpy(req->buf, hidmouse_report_desc, length);
			goto respond;
		default:
			goto stall;
		}
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_GET_REPORT):
		/* no motion, no buttons */
		length = min_t(u16, length, HIDMOUSE_REPORT_LEN);
		memset(req->buf, 0, length);
		goto respond;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_GET_PROTOCOL):
		length = min_t(u16, length, 1);
		((u8 *)req->buf)[0] = hidmouse->protocol;
		goto respond;

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_SET_PROTOCOL):
		if (value > HID_REPORT_PROTOCOL)
			goto stall;
		/* boot layout == our report layout, so nothing else changes */
		hidmouse->protocol = value;
		length = 0;
		goto respond;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_GET_IDLE):
		length = min_t(u16, length, 1);
		((u8 *)req->buf)[0] = hidmouse->idle;
		goto respond;

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_SET_IDLE):
		/* duration is the high byte of wValue */
		hidmouse->idle = value >> 8;
		length = 0;
		goto respond;

	default:
		goto stall;
	}

stall:
	/* composite stalls ep0 for us */
	return -EOPNOTSUPP;

respond:
	req->zero = 0;
	req->length = length;
	status = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
	if (status < 0)
		ERROR(cdev, "usb_ep_queue error on ep0 %d\n", status);
	return status;
}

static void hidmouse_free(struct usb_function *f)
{
	kfree(func_to_hidmouse(f));
}

/*-------------------------------------------------------------------------*/
/*                           function instance                             */

static void hidmouse_inst_release(struct config_item *item)
{
	usb_put_function_instance(container_of(to_config_group(item),
				  struct usb_function_instance, group));
}

static struct configfs_item_operations hidmouse_item_ops = {
	.release	= hidmouse_inst_release,
};

static const struct config_item_type hidmouse_func_type = {
	.ct_item_ops	= &hidmouse_item_ops,
	.ct_owner	= THIS_MODULE,
};

static void hidmouse_free_inst(struct usb_function_instance *fi)
{
	kfree(fi);
}

static struct usb_function_instance *hidmouse_alloc_inst(void)
{
	struct usb_function_instance *fi;

	fi = kzalloc(sizeof(*fi), GFP_KERNEL);
	if (!fi)
		return ERR_PTR(-ENOMEM);
	fi->free_func_inst = hidmouse_free_inst;

	config_group_init_type_name(&fi->group, "", &hidmouse_func_type);

	return fi;
}

static struct usb_function *hidmouse_alloc(struct usb_function_instance *fi)
{
	struct f_hidmouse *hidmouse;

	hidmouse = kzalloc(sizeof(*hidmouse), GFP_KERNEL);
	if (!hidmouse)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&hidmouse->lock);

	hidmouse->func.name      = "hidmouse";
	hidmouse->func.bind      = hidmouse_bind;
	hidmouse->func.unbind    = hidmouse_unbind;
	hidmouse->func.set_alt   = hidmouse_set_alt;
	hidmouse->func.disable   = hidmouse_disable;
	hidmouse->func.setup     = hidmouse_setup;
	hidmouse->func.free_func = hidmouse_free;

	return &hidmouse->func;
}

DECLARE_USB_FUNCTION_INIT(hidmouse, hidmouse_alloc_inst, hidmouse_alloc);
MODULE_LICENSE("GPL");
