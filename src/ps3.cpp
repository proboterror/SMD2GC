#include "ps3.h"
#include "tusb.h"

/*
Ported from https://github.com/proboterror/HIDman_ZX/blob/main/firmware/ps3.c

Reference:
https://github.com/felis/USB_Host_Shield_2.0/PS3USB.cpp 
https://github.com/Slamy/Yaumataca/src/handlers/hid_ps3.cpp
https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c

VendorID: 0x054C
ProductID: 0x0268

Device Descriptor :
12 01 00 02 00 00 00 40 4C 05 68 02 00 01 01 02
00 01

Config Descriptor :
09 02 29 00 01 01 00 80 FA 09 04 00 00 02 03 00
00 00 09 21 11 01 00 01 22 94 00 07 05 02 03 40
00 01 07 05 81 03 40 00 01

Interface 0 Report Descriptor :
05 01 09 04 A1 01 A1 02 85 01 75 08 95 01 15 00
26 FF 00 81 03 75 01 95 13 15 00 25 01 35 00 45
01 05 09 19 01 29 13 81 02 75 01 95 0D 06 00 FF
81 03 15 00 26 FF 00 05 01 09 01 A1 00 75 08 95
04 35 00 46 FF 00 09 30 09 31 09 32 09 35 81 02
C0 05 01 75 08 95 27 09 01 81 02 75 08 95 30 09
01 91 02 75 08 95 30 09 01 B1 02 C0 A1 02 85 02
75 08 95 30 09 01 B1 02 C0 A1 02 85 EE 75 08 95
30 09 01 B1 02 C0 A1 02 85 EF 75 08 95 30 09 01
B1 02 C0 C0
*/

#define PS3_VID 0x054C  // Sony Corporation
#define PS3_PID 0x0268  // PS3 Controller DualShock 3

#define PS3_FEATURE_ID 0xF4
#define PS3_REPORT_LEN 49

bool ps3_usb_match(uint16_t vendor_id, uint16_t product_id)
{
	return (vendor_id == PS3_VID) && (product_id == PS3_PID);
}

bool ps3_usb_init(uint8_t dev_addr, uint8_t instance)
{
	// Command used to enable the Dualshock 3 and Navigation controller to send data via USB
	static uint8_t enable_command[] = { 0x42, 0x0c, 0x00, 0x00 };

	const uint8_t report_id = 0xF4;

	tusb_control_request_t const request = {
				.bmRequestType_bit = {
					.recipient = TUSB_REQ_RCPT_INTERFACE,
					.type      = TUSB_REQ_TYPE_CLASS,
					.direction = TUSB_DIR_OUT
				},
				.bRequest = HID_REQ_CONTROL_SET_REPORT,
				.wValue = tu_htole16((HID_REPORT_TYPE_FEATURE << 8) | report_id),
				.wIndex = tu_htole16(instance), // interface number, 0 for DualShock 3
				.wLength = sizeof(enable_command)
	};

	tuh_xfer_t xfer = {
		.daddr       = dev_addr,
		.ep_addr     = 0, // control endpoint
		.setup       = &request,
		.buffer      = enable_command,
		.complete_cb = NULL,
		.user_data   = 0
	};

	return tuh_control_xfer(&xfer);
}

ps3_hid_report_t* ps3_usb_parse_report(const uint8_t* report, uint16_t len)
{
	if(len != PS3_REPORT_LEN)
		return nullptr;

	return (ps3_hid_report_t*) report;
}