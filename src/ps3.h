#pragma once

#include <stdint.h>

typedef struct
{
	uint8_t reserved1[2];

	// Byte 2
	uint8_t button_select : 1;
	uint8_t stick_click_left : 1;
	uint8_t stick_click_right : 1;
	uint8_t button_start : 1;
	uint8_t dpad_up : 1;
	uint8_t dpad_right : 1;
	uint8_t dpad_down : 1;
	uint8_t dpad_left : 1;

	// Byte 3
	uint8_t trigger_l2 : 1;
	uint8_t trigger_r2 : 1;
	uint8_t trigger_l1 : 1;
	uint8_t trigger_r1 : 1;

	uint8_t button_triangle : 1;
	uint8_t button_circle : 1;
	uint8_t button_cross : 1;
	uint8_t button_square : 1;

	// Bytes 4,5
	uint8_t padding1[2];

	// Bytes 6,7,8,9
	uint8_t joy_left_x;
	uint8_t joy_left_y;
	uint8_t joy_right_x;
	uint8_t joy_right_y;

    // Bytes 10-13
    uint8_t padding2[4]; // Checked with CECHZC2R controller; most sources claims 3-byte padding.

    // Byte 14 to byte 25
	uint8_t dpad_up_analog;
	uint8_t dpad_right_analog;
	uint8_t dpad_down_analog;
	uint8_t dpad_left_analog;

	uint8_t trigger_l2_analog;
	uint8_t trigger_r2_analog;
	uint8_t trigger_l1_analog;
	uint8_t trigger_r1_analog;

	uint8_t button_triangle_analog;
	uint8_t button_circle_analog;
	uint8_t button_cross_analog;
	uint8_t button_square_analog;
} ps3_hid_report_t;

bool ps3_usb_match(uint16_t vendor_id, uint16_t product_id);
bool ps3_usb_init(uint8_t dev_addr, uint8_t instance);
ps3_hid_report_t* ps3_usb_parse_report(const uint8_t* report, uint16_t len);