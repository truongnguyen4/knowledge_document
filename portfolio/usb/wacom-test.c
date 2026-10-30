// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  HID driver for Wacom Intuos BT S (VID 0x2D1F, PID 0x03C5) - test driver
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/input.h>

#define VENDOR_ID_WACOM		    0x2d1f
#define DEVICE_ID_WACOM         0x03c5
#define VENDOR_ID_XPPEN         0x28bd
#define DEVICE_ID_XPPEN         0x1140

struct mouse_data {
    s32 x;
    s32 y;
    bool inrange;
    bool tipswitch;
    s32 keyboard;
};

static int wt_event(struct hid_device *hdev, struct hid_field *field,
                    struct hid_usage *usage, __s32 value)
{
    struct mouse_data *mouse_dat = hid_get_drvdata(hdev);
    struct input_dev *input = field->hidinput->input;

    switch (usage->hid) {
    case HID_GD_X:
        if (!mouse_dat->inrange)
            return 1;   /* ignore movements when out of range */
        input_report_rel(input, REL_X, (value - mouse_dat->x) / 8);
        mouse_dat->x = value;
        return 1;
    case HID_GD_Y:
        if (!mouse_dat->inrange)
            return 1;   /* ignore movements when out of range */
        input_report_rel(input, REL_Y, (value - mouse_dat->y) / 8);
        mouse_dat->y = value;
        return 1;
    case HID_DG_TIPSWITCH:
        mouse_dat->tipswitch = value;
        input_report_key(input, BTN_LEFT, mouse_dat->tipswitch);
        return 1;
    case HID_DG_INRANGE:
        mouse_dat->inrange = value;
        return 1;
    case HID_DG_BARRELSWITCH:
        input_report_key(input, BTN_TL, value);
        return 1;
    case HID_DG_BARRELSWITCH2:
        input_report_key(input, BTN_TR, value);
        return 1;
    case HID_UP_KEYBOARD:
        mouse_dat->keyboard = value;
        input_report_key(input, BTN_TL, mouse_dat->keyboard);
        return 1;
    default:
        return 0;
    }
}

static int wt_input_mapping(struct hid_device *hdev, struct hid_input *input,
                            struct hid_field *field, struct hid_usage *usage,
                            unsigned long **bit, int *max)
{
    switch (usage->hid) {
	case HID_GD_X:
		hid_map_usage(input, usage, bit, max, EV_REL, REL_X);
		return 1;
	case HID_GD_Y:
		hid_map_usage(input, usage, bit, max, EV_REL, REL_Y);
		return 1;
	case HID_DG_TIPSWITCH:
		hid_map_usage(input, usage, bit, max, EV_KEY, BTN_LEFT);
		return 1;
    case HID_DG_INRANGE:
        return 1;
	case HID_DG_BARRELSWITCH:
		hid_map_usage(input, usage, bit, max, EV_KEY, BTN_TL);
		return 1;
	case HID_DG_BARRELSWITCH2:
		hid_map_usage(input, usage, bit, max, EV_KEY, BTN_TR);
		return 1;
    case HID_UP_KEYBOARD:
		hid_map_usage(input, usage, bit, max, EV_KEY, BTN_TL);
        return 1;
	default:
		return -1;
	}
    return 0;
}

/* Whole report, after every field->value[] has been filled */
static void wt_report(struct hid_device *hdev, struct hid_report *report)
{
    size_t s = 0; 
    size_t u = 0;
    struct hid_field *field;
    s32 x = 0, y = 0;
    bool inrange = false, tipswitch = false, trig_left = false, trig_right = false;
    
    s32 keyboard = 0;
    
    dev_info(&hdev->dev, "ttnguyen4: report: id %d\n", report->id);
    if (report->id == 7) {
        for (s = 0; s < report->maxfield; s++) {
            field = report->field[s];
            for (u = 0; u < field->maxusage; u++) {
                switch (field->usage[u].hid) {
                case HID_GD_X:
                    x = field->value[u];
                    break;
                case HID_GD_Y:
                    y = field->value[u];
                    break;
                case HID_DG_INRANGE:
                    inrange = field->value[u];
                    break;
                case HID_DG_TIPSWITCH:
                    tipswitch = field->value[u];
                    break;
                case HID_DG_BARRELSWITCH:
                    trig_left = field->value[u];
                    break;
                case HID_DG_BARRELSWITCH2:
                    trig_right = field->value[u];
                    break;
                }
            }
        }
        dev_info(&hdev->dev, "ttnguyen4: X = %d, Y = %d, InRange = %d, TipSwitch = %d, TrigLeft = %d, TrigRight = %d\n", x, y, inrange, tipswitch, trig_left, trig_right);
        return;
    }

    if (report->id == 6) {
        for (s = 0; s < report->maxfield; s++) {
            field = report->field[s];
            for (u = 0; u < field->maxusage; u++) {
                switch (field->usage[u].hid) {
                case HID_UP_KEYBOARD:
                    keyboard = field->value[u];
                    break;
                }
            }
        }
        dev_info(&hdev->dev, "ttnguyen4: keyboard = %d\n", keyboard);
    }
}

static int wt_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
    struct mouse_data *mouse_dat;
    
	mouse_dat = devm_kzalloc(&hdev->dev, sizeof(*mouse_dat), GFP_KERNEL);

	if (!mouse_dat) {
		return -ENOMEM;
    }
	hid_set_drvdata(hdev, mouse_dat);

    // Parse the HID report descriptor
	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "ttnguyen4: parse failed\n");
		return ret;
	}

    // Start the HID hardware and connect it to the default interface
	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "ttnguyen4: hw start failed\n");
		return ret;
	}

    // Open the HID hardware for communication
    ret = hid_hw_open(hdev);
    if (ret) {
        hid_err(hdev, "ttnguyen4: hw open failed\n");
        return ret;
    }

	return 0;
}

static void wt_remove(struct hid_device *hdev)
{
    // Stop the HID hardware and close communication
    hid_hw_close(hdev);
    hid_hw_stop(hdev);
}

static const struct hid_device_id wt_devices[] = {
	{ HID_USB_DEVICE(VENDOR_ID_WACOM, DEVICE_ID_WACOM) },
    { HID_USB_DEVICE(VENDOR_ID_XPPEN, DEVICE_ID_XPPEN) },
    /* Consider find class/subclass/protocol for more generic matching */
	{ }
};

MODULE_DEVICE_TABLE(hid, wt_devices);

static struct hid_driver wt_driver = {
	.name = "wacom-test",
	.id_table = wt_devices,
	.probe = wt_probe,
	.remove = wt_remove,
	.report = wt_report,
    .input_mapping = wt_input_mapping,
    .event = wt_event,
};
module_hid_driver(wt_driver);

MODULE_DESCRIPTION("HID test driver for Wacom Intuos BT S (2D1F:03C5)");
MODULE_LICENSE("GPL");
