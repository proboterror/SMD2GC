#pragma once

#include <stdint.h>

/*
USB HID gamepad / joystick report descriptor parser / device reports mapper.
Based on https://github.com/rasteri/HIDman/blob/main/firmware/parsedescriptor.c
For https://github.com/proboterror/SMD2GC

Features:
- Support USB HID gamepads reports parsing and conversion to user pad format / mapping to keyboard with preset table
- Also support USB HID keyboard and mouse descriptors and reports parsing with user callbacks
- C++ library in C style, can be adapted for embedded C compilers with minimal changes
- Transparent axis values conversion from / to int8_t, uint8_t, int16_t, uint16_t ranges
- No memory fragmentation, 1 kByte static memory for arena allocator
- Unit tests for validation and refactoring

How to use:

- Declare enum with user gamepad controls ids:
enum my_mappings : uint8_t
{
	MAP_MY_PAD_BUTTON_A,
	MAP_MY_PAD_BUTTON_B,
	MAP_MY_PAD_R,
	MAP_MY_PAD_L,
	MAP_MY_PAD_D,
	MAP_MY_PAD_U,
	MAP_MY_PAD_AXIS_X,
	MAP_MY_PAD_AXIS_Y
};

- Declare mapping table from HID report data values / types to user types:
JoyPreset hid_to_my_pad_mapping[] =
{
	// Pad Number, Input Usage Page, Input Usage, Output Channel, Output Control, InputType, Input Param
	// Buttons
	{ 1, REPORT_USAGE_PAGE_BUTTON, 1, MAP_GAMEPAD, MAP_MY_PAD_BUTTON_A, MAP_TYPE_THRESHOLD_ABOVE, 0 }, // HID Button 1 -> Button A
	{ 1, REPORT_USAGE_PAGE_BUTTON, 2, MAP_GAMEPAD, MAP_MY_PAD_BUTTON_B, MAP_TYPE_THRESHOLD_ABOVE, 0 }, // HID Button 2 -> Button B

	// Left analog mapped to directions buttons (axis theshold values mapped to 0..255 range)
	{ 1, REPORT_USAGE_PAGE_GENERIC_DESKTOP, REPORT_USAGE_X, MAP_GAMEPAD, MAP_MY_PAD_R, MAP_TYPE_THRESHOLD_ABOVE, 192 },
	{ 1, REPORT_USAGE_PAGE_GENERIC_DESKTOP, REPORT_USAGE_X, MAP_GAMEPAD, MAP_MY_PAD_L, MAP_TYPE_THRESHOLD_BELOW, 64 },
	{ 1, REPORT_USAGE_PAGE_GENERIC_DESKTOP, REPORT_USAGE_Y, MAP_GAMEPAD, MAP_MY_PAD_U, MAP_TYPE_THRESHOLD_ABOVE, 192 },
	{ 1, REPORT_USAGE_PAGE_GENERIC_DESKTOP, REPORT_USAGE_Y, MAP_GAMEPAD, MAP_MY_PAD_D, MAP_TYPE_THRESHOLD_BELOW, 64 },

	// Right analog mapped to user pad analog X/Y axes in 0.255 range (uint8_t target values)
	{ 1, REPORT_USAGE_PAGE_GENERIC_DESKTOP, REPORT_USAGE_X, MAP_GAMEPAD, MAP_MY_PAD_AXIS_X, MAP_TYPE_AXIS, VALUE_TYPE_UINT8 },
	{ 1, REPORT_USAGE_PAGE_GENERIC_DESKTOP, REPORT_USAGE_Y, MAP_GAMEPAD, MAP_MY_PAD_AXIS_Y, MAP_TYPE_AXIS, VALUE_TYPE_UINT8 },

	// null record to mark end
	{ 0, 0, 0, 0, 0, 0, 0 }
};

- Declare user callback for parsing report data (handles buttons presses and axis values)
typedef struct my_gamepad_t
{
	bool u, d, l, r;
	bool a, b;
	uint8_t x, y;
} my_gamepad;

my_gamepad gamepad;

void gamepad_callback(uint32_t control_type, uint32_t value)
{
	const my_mappings mapping = (my_mappings)control_type;

	if (mapping == MAP_MY_PAD_BUTTON_A)
		gamepad.a = true;
	else if(mapping == MAP_MY_PAD_BUTTON_B)
		gamepad.b = true;
	else if(mapping == MAP_MY_PAD_R)
		gamepad.r = true;
	else if(mapping == MAP_MY_PAD_L)
		gamepad.l = true;
	else if(mapping == MAP_MY_PAD_D)
		gamepad.d = true;
	else if(mapping == MAP_MY_PAD_U)
		gamepad.u = true;
	else if(mapping == MAP_MY_PAD_AXIS_X)
		gamepad.x = value;
	else if(mapping == MAP_MY_PAD_AXIS_Y)
		gamepad.y = value;
}

- Call in code:
	ParseReportDescriptor(hid_report_descriptor, sizeof(hid_report_descriptor), hid_to_my_pad_mapping);

	while(true)
	{
		gamepad = {};
		ParseReport(sizeof(hid_report), hid_report, gamepad_callback);
	}

- Keyboard and mouse support:

- Declare keyboard / mouse callbacks:

typedef struct generic_mouse_t
{
	uint16_t x, y;
	int16_t z;
	uint8_t buttons;
} generic_mouse;

generic_mouse g_mouse;

bool g_keyboard[256];

void keyboard_callback(uint8_t hid_code, bool state)
{
	g_keyboard[hid_code] = state;
}

void mouse_callback(int16_t dx, int16_t dy, int16_t dz, uint8_t buttons)
{
	g_mouse.x += dx;
	g_mouse.y += dy;
	g_mouse.z += dz;

	g_mouse.buttons = buttons;
}

- Pass keyboard_callback / mouse_callback to ParseReport.
*/

// USB Device Class Definition for Human Interface Devices(HID) Version 1.11
// https://www.usb.org/sites/default/files/documents/hid1_11.pdf

// Collection Item - HID 1.11 section 6.2.2.6
enum hid_collection_item
{
	HID_COLLECTION_PHYSICAL_ = 0,
	HID_COLLECTION_APPLICATION_
/*
	HID_COLLECTION_LOGICAL,
	HID_COLLECTION_REPORT,
	HID_COLLECTION_NAMED_ARRAY,
	HID_COLLECTION_USAGE_SWITCH,
	HID_COLLECTION_USAGE_MODIFIER
*/
};

// Short Items - HID 1.11 section 6.2.2.2
enum hid_item_type
{
	HID_TYPE_MAIN = 0,
	HID_TYPE_GLOBAL = 1,
	HID_TYPE_LOCAL = 2
};

// Long items - HID 1.11 section 6.2.2.3
enum hid_long_item_tag
{
	HID_ITEM_TAG_LONG = 0x0F
};

// Main Items - HID 1.11 section 6.2.2.4
enum hid_main_item_tag
{
	HID_MAIN_ITEM_TAG_INPUT = 0x08,
	HID_MAIN_ITEM_TAG_COLLECTION_START = 0x0A,
	HID_MAIN_ITEM_TAG_COLLECTION_END = 0x0C
};

// Main Items - HID 1.11 section 6.2.2.4
// Main item tag == Input, item data bit 1 flag
#define HID_INPUT_VARIABLE 0x02

// Local Items - HID 1.11 section 6.2.2.8
enum hid_local_item_tag
{
	HID_LOCAL_ITEM_TAG_USAGE = 0x00,
	HID_LOCAL_ITEM_TAG_USAGE_MIN = 0x01,
	HID_LOCAL_ITEM_TAG_USAGE_MAX = 0x02,
};

// Global Items - HID 1.11 section 6.2.2.7
enum hid_global_item_tag
{
	HID_GLOBAL_ITEM_TAG_USAGE_PAGE = 0x00,
	HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM = 0x01,
	HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM = 0x02,
	HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM = 0x03,
	HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM = 0x04,
	HID_GLOBAL_ITEM_TAG_REPORT_SIZE = 0x07,
	HID_GLOBAL_ITEM_TAG_REPORT_ID = 0x08,
	HID_GLOBAL_ITEM_TAG_REPORT_COUNT = 0x09
	// GLOBAL_PUSH = 10,
	// GLOBAL_POP = 11
};

// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf
// USB HID Usage Tables Version 1.12

// HID Usage Table - Table 1: Usage Page Summary
enum hid_usage_pages
{
	REPORT_USAGE_PAGE_GENERIC_DESKTOP = 0x01,
	REPORT_USAGE_PAGE_KEYBOARD = 0x07,
	REPORT_USAGE_PAGE_LEDS = 0x08,
	REPORT_USAGE_PAGE_BUTTON = 0x09,
	REPORT_USAGE_PAGE_VENDOR = 0xFF00
};

// 4 Generic Desktop Page (0x01)
// HID Usage Table - Table 6: Generic Desktop Page
enum generic_page_input_usage : uint8_t
{
	REPORT_USAGE_UNKNOWN = 0x00,
	REPORT_USAGE_POINTER = 0x01,
	REPORT_USAGE_MOUSE = 0x02,
	REPORT_USAGE_RESERVED = 0x03,
	REPORT_USAGE_JOYSTICK = 0x04,
	REPORT_USAGE_GAMEPAD = 0x05,
	REPORT_USAGE_KEYBOARD = 0x06,
	REPORT_USAGE_KEYPAD = 0x07,
	REPORT_USAGE_MULTI_AXIS = 0x08,
	REPORT_USAGE_SYSTEM = 0x09,
	// ...
	REPORT_USAGE_X = 0x30,
	REPORT_USAGE_Y = 0x31,
	REPORT_USAGE_Z = 0x32,
	REPORT_USAGE_Rx = 0x33,
	REPORT_USAGE_Ry = 0x34,
	REPORT_USAGE_Rz = 0x35,
	REPORT_USAGE_WHEEL = 0x38,
	REPORT_USAGE_HATSWITCH = 0x39,
	//...
	// Rarely used
	REPORT_USAGE_DPAD_UP = 0x90,
	REPORT_USAGE_DPAD_DOWN = 0x91,
	REPORT_USAGE_DPAD_RIGHT = 0x92,
	REPORT_USAGE_DPAD_LEFT = 0x93
};

// Hat switch directions codes not described in HID usage tables specification.
enum hid_gamepad_hat
{
	HID_GAMEPAD_HAT_UP = 0,
	HID_GAMEPAD_HAT_UP_RIGHT = 1,
	HID_GAMEPAD_HAT_RIGHT = 2,
	HID_GAMEPAD_HAT_DOWN_RIGHT = 3,
	HID_GAMEPAD_HAT_DOWN = 4,
	HID_GAMEPAD_HAT_DOWN_LEFT = 5,
	HID_GAMEPAD_HAT_LEFT = 6,
	HID_GAMEPAD_HAT_UP_LEFT = 7,
	HID_GAMEPAD_HAT_CENTERED = 8,
};

// Non-HID constants: user report data mapping constants
// Mouse mapping constants, unused
enum mouse_mapping
{
	MAP_MOUSE_BUTTON1 = 1, // HID mouse report buttons usage minimum = 1 (left button)
	MAP_MOUSE_BUTTON2,
	MAP_MOUSE_BUTTON3,
	MAP_MOUSE_BUTTON4,
	MAP_MOUSE_BUTTON5,
	MAP_MOUSE_X,
	MAP_MOUSE_Y,
	MAP_MOUSE_WHEEL
};

// Describe how to process input value from HID report
enum preset_input_type : uint8_t
{
	MAP_TYPE_NONE = 0,
	MAP_TYPE_THRESHOLD_BELOW,
	MAP_TYPE_THRESHOLD_ABOVE,
	MAP_TYPE_SCALE,
	MAP_TYPE_ARRAY,
	MAP_TYPE_BITFIELD,
	MAP_TYPE_EQUAL,
	MAP_TYPE_AXIS
};

// Decribe HID device type to map from
enum preset_output_channel : uint8_t
{
	MAP_KEYBOARD,
	MAP_MOUSE,
	MAP_GAMEPAD
};

// User value type / range to map to from report value min / max range
enum preset_value_type
{
	VALUE_TYPE_UINT8,
	VALUE_TYPE_INT8,
	VALUE_TYPE_UINT16,
	VALUE_TYPE_INT16,
	VALUE_TYPE_CUSTOM
};

typedef struct JoyPreset
{
	// Input HID joystick or gamepad index.
	// Note: USB HID report descriptor can describe more than one Report ID or gamepad.
	uint8_t number;

	uint8_t inputUsagePage; // hid_usage_pages::REPORT_USAGE_PAGE_BUTTON / REPORT_USAGE_PAGE_GENERIC_DESKTOP

	uint32_t inputUsage; // button index or generic_page_input_usage::REPORT_USAGE_Y / REPORT_USAGE_HATSWITCH

	// Mouse or keyboard or gamepad
	uint8_t outputChannel; // preset_output_channel::MAP_GAMEPAD

	// for keyboard, this is the HID scancode of the key associated with this control
	// for gamepad, user controller button/axis ID
	uint8_t outputControl;

	// How this control gets interpreted - preset_input_type::MAP_TYPE_x
	uint8_t inputType;

	// Param has different meanings depending on InputType:
	// - reference value to compare report data with for button state for push buttons / d-pad / hat switch
	// - value_type for converting axis type / range with MAP_TYPE_AXIS
	uint16_t inputParam;
} JoyPreset;

typedef void (*gamepad_callback_t)(uint32_t control_type, uint32_t value);
typedef void (*keyboard_callback_t)(uint8_t hid_code, bool state);
typedef void (*mouse_callback_t)(int16_t dx, int16_t dy, int16_t dz, uint8_t buttons);

bool ParseReportDescriptor(const uint8_t* descriptor, const uint16_t len, const JoyPreset* preset);
bool ParseReport(const uint8_t* report, uint32_t len,
	gamepad_callback_t gamepad_callback, keyboard_callback_t keyboard_callback = nullptr, mouse_callback_t mouse_callback = nullptr);

#define map_to_uint8(value, min, max) (uint8_t)((((value - min) * 0xFF) + ((max - min) >> 1)) / (max - min))

// Convert value range from HID report minimum / maximum range to target type range.
// uint8, int8, uint16, int16 ranges supported for input/output.
// int32_t, uint32_t ranges not supported for input/output.
// Custom ranges (like 1..16, 1..12000) not supported.
// value can be signed or unsigned in 2's complement, flagged by minimum < 0.
uint32_t convert_range(const uint32_t value, const int16_t minimum, const uint16_t maximum, const preset_value_type target_type);
