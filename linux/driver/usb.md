# USB framework

## Overview

The USB framework in Linux consists of three main layers: 
- the class/function drivers
- the USB core
- the host controller drivers.
The host-driven nature of USB requires the host to initiate all communication with devices.

| Layer | Code | Role |
| --- | --- | --- |
| Class / function driver | `usbhid`, `usb-storage`, `cdc_acm`, `usbmouse`, ... | The **user**: knows the protocol of one interface, asks for data and consumes it |
| USB core (`usbcore`) | `drivers/usb/core/` | The **brain**: control requests to learn what the device is, parses descriptors, picks the driver, hands requests down |
| Host controller driver (HCD) | `xhci-hcd`, `dwc3`, `ehci-hcd` | The **hands**: moves the request onto the wire and talks to the device at bit/byte level |

## Architecture
![usb framework overview](resources/usb_framework_overview.png)

## Sequence diagram (dynamic load driver)
![USB host sequence diagram source](resources/usb_sequence_host.png)

## Transfer types
| Type | Idea | Guarantee | Retry on error | Typical use |
| --- | --- | --- | --- | --- |
| Control | Ask the device about itself and configure it | Delivery | Yes | Enumeration, class requests (`usb_control_msg`) |
| Bulk | Move a big chunk of data correctly, whenever the bus is free | Delivery, no latency bound | Yes | Mass storage, CDC data |
| Interrupt | Host polls the endpoint at a fixed period | Delivery | Yes (next period) | HID input (mouse, keyboard) |
| Isochronous | Data on a fixed period, correctness traded for timing | Bandwidth/latency, **no** delivery | No | Audio, video |

## 1. Descriptors

```
usb_device_descriptor                       (product information: vendor, product, USB version)
 └─ usb_config_descriptor                   (device configuration)
     ├─ usb_interface_assoc_descriptor      (interfaces belong to the same function)
     └─ usb_interface_descriptor            (one function of the device)
         ├─ class descriptor                (extra info defined by that class, e.g. HID, CDC)
         └─ usb_endpoint_descriptor         (the data pipes: direction, transfer type, size, speed)
```

| Descriptor | bDescriptorType | Struct | Parsed by | Meaning |
| --- | --- | --- | --- | --- |
| DEVICE | 1 | `usb_device_descriptor` | `usb_get_device_descriptor()` | VID/PID, USB version, EP0 size |
| CONFIG | 2 | `usb_config_descriptor` | `usb_get_configuration()` | Power budget, number of interfaces |
| INTERFACE ASSOCIATION | 11 | `usb_interface_assoc_descriptor` | `usb_parse_configuration()` | Grouped interfaces (one function) |
| INTERFACE | 4 | `usb_interface_descriptor` | `usb_parse_interface()` | Class/subclass/protocol → driver matching |
| ENDPOINT | 5 | `usb_endpoint_descriptor` | `usb_parse_endpoint()` | Direction, transfer type, max packet, interval |
| HID CLASS | 33 | `usb_hid_descriptor` | `usbhid` | Report descriptor length |

## 2. Driver binding
### Filter interfaces
**The driver does not choose the device, the match does.** A driver only declares a filter in its `id_table`
	- concrete VID/PID
	- a class/subclass/protocol
`probe()` is called once per matching **interface**.

**usbcore never pushes data on its own.**
- After `probe()` the driver must submit an URB

```c
static const struct usb_device_id my_ids[] = {
	{ USB_DEVICE(0x2d1f, 0x03c5) },                    /* VID/PID match */
	{ USB_INTERFACE_INFO(USB_CLASS_HID, 0, 0) },       /* class match */
	{ }
};
MODULE_DEVICE_TABLE(usb, my_ids);

static struct usb_driver my_driver = {
	.name       = "my-usb",
	.id_table   = my_ids,
	.probe      = my_probe,   
	.disconnect = my_disconnect,
};
module_usb_driver(my_driver);
```

### Request URB 
```c
/* probe ------------------------------------------------------------ */
static void my_irq_in(struct urb *urb)   /* softirq context, must not sleep */
{
	/* TODO: Handle the interrupt */

	/* Resubmit the URB to keep polling */
	usb_submit_urb(urb, GFP_ATOMIC);      /* keep the bandwidth reservation */
}

usb_fill_int_urb(urb, udev,
		 usb_rcvintpipe(udev, bEndpointAddress),
		 buf, size, my_irq_in, priv, bInterval);

usb_submit_urb(priv->urb, GFP_KERNEL);   /* starts the polling */
```

## 3. Debug
### Interface information:
Check bus usb:
`ls /sys/bus/usb/`
- `devices/`: discovered interfaces 
- `drivers`: usb drivers

```sh
# interfaces
ls /sys/bus/usb/devices/                  # devices (2-1) and interfaces (2-1:1.0)
# interface 1-1:0.1 information
ls /sys/bus/usb/devices/1-1:0.1/
# drivers
ls /sys/bus/usb/drivers/                  # one dir per usb_driver
# driver is binding with interface
readlink -f /sys/bus/usb/devices/2-1:1.0/driver   # which driver owns this interface

# bind/unbind driver wi
echo -n "2-1:1.0" > /sys/bus/usb/drivers/usbhid/unbind
echo -n "2-1:1.0" > /sys/bus/usb/drivers/usbhid/bind
```

Script list all interfaces and corresponding drivers
```sh
# Summary of every interface: class, endpoints, bound driver
for i in /sys/bus/usb/devices/*:*; do
  echo "$(basename $i) class=$(cat $i/bInterfaceClass) subclass=$(cat $i/bInterfaceSubClass) protocol=$(cat $i/bInterfaceProtocol) alt=$(cat $i/bAlternateSetting) nep=$(cat $i/bNumEndpoints) driver=$(basename $(readlink -f $i/driver))"
done
```


### HID specific
```sh
cat /sys/kernel/debug/hid/0003:2D1F:03C5.0013/rdesc    # report descriptor + parsed fields
cat /sys/kernel/debug/hid/0003:2D1F:03C5.0013/events   # live reports from hid_irq_in()
```
---

```sh
lsusb -t                                     # topology + bound driver per interface
lsusb -v -d 2d1f:03c5                        # transfer type + bInterval
cat /sys/kernel/debug/usb/devices   
```


## 4. Descriptor structs
Definitions from `include/uapi/linux/usb/ch9.h`
``` c
struct usb_device_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 bcdUSB;
	__u8  bDeviceClass;
	__u8  bDeviceSubClass;
	__u8  bDeviceProtocol;
	__u8  bMaxPacketSize0;
	__le16 idVendor;
	__le16 idProduct;
	__le16 bcdDevice;
	__u8  iManufacturer;
	__u8  iProduct;
	__u8  iSerialNumber;
	__u8  bNumConfigurations;
};

struct usb_config_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 wTotalLength;
	__u8  bNumInterfaces;
	__u8  bConfigurationValue;
	__u8  iConfiguration;
	__u8  bmAttributes;
	__u8  bMaxPower;
};

struct usb_interface_assoc_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bFirstInterface;
	__u8  bInterfaceCount;
	__u8  bFunctionClass;
	__u8  bFunctionSubClass;
	__u8  bFunctionProtocol;
	__u8  iFunction;
};

struct usb_interface_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bInterfaceNumber;
	__u8  bAlternateSetting;
	__u8  bNumEndpoints;
	__u8  bInterfaceClass;
	__u8  bInterfaceSubClass;
	__u8  bInterfaceProtocol;
	__u8  iInterface;
};

struct usb_endpoint_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bEndpointAddress;
	__u8  bmAttributes;
	__le16 wMaxPacketSize;
	__u8  bInterval;
	__u8  bRefresh;
	__u8  bSynchAddress;
};
```


```sh
xxd /sys/bus/usb/devices/2-1/descriptors
00000000: 1201 0002 0000 0040 1f2d c503 0702 0102  .......@.-......
00000010: 0301 0902 2200 0101 0080 fa09 0400 0001  ...."...........
00000020: 0300 0000 0921 1001 0001 22b7 0007 0581  .....!....".....
00000030: 0340 0001                                .@..
```
