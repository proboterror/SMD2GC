#include <cassert>
#include <stdio.h>

#include "hid_dumps.h"
#include "hid_parser.h"

#include "hid_gamecube_mapping.h"

typedef struct generic_gamepad_t
{
	bool start;
	bool u, d, l, r;
	bool a, b, x, y;
	bool l1, r1, l2, r2;
	uint8_t al, ar;
	uint8_t lx, ly, rx, ry;
} generic_gamepad;

static generic_gamepad g_gamepad;

typedef struct generic_mouse_t
{
	uint16_t x, y;
	int16_t z;
	uint8_t buttons;
} generic_mouse;

static generic_mouse g_mouse;

typedef struct generic_keyboard_t
{
	bool keys[256];
} generic_keyboard;

static generic_keyboard g_keyboard;

static void keyboard_callback(uint8_t hid_code, bool state)
{
	g_keyboard.keys[hid_code] = state;
}

static void mouse_callback(int16_t dx, int16_t dy, int16_t dz, uint8_t buttons)
{
	g_mouse.x += dx;
	g_mouse.y += dy;
	g_mouse.z += dz;

	g_mouse.buttons = buttons;
}

static void gamepad_callback(uint32_t control_type, uint32_t value)
{
	const GamecubeMappings mapping = (GamecubeMappings)control_type;
	printf("gamepad_callback: type=%d, value=%d\n", control_type, value);

	if (mapping == MAP_GAMECUBE_BUTTON_A)
		g_gamepad.a = true;
	else if(mapping == MAP_GAMECUBE_BUTTON_B)
		g_gamepad.b = true;
	else if(mapping == MAP_GAMECUBE_BUTTON_X)
		g_gamepad.x = true;
	else if(mapping == MAP_GAMECUBE_BUTTON_Y)
		g_gamepad.y = true;
	else if(mapping == MAP_GAMECUBE_BUTTON_START)
		g_gamepad.start = true;
	else if(mapping == MAP_GAMECUBE_R)
		g_gamepad.r = true;
	else if(mapping == MAP_GAMECUBE_L)
		g_gamepad.l = true;
	else if(mapping == MAP_GAMECUBE_D)
		g_gamepad.d = true;
	else if(mapping == MAP_GAMECUBE_U)
		g_gamepad.u = true;
	else if(mapping == MAP_GAMECUBE_BUTTON_Z)
		g_gamepad.r1 = true;
	else if(mapping == MAP_GAMECUBE_BUTTON_R)
		g_gamepad.r2 = true;
	else if(mapping == MAP_GAMECUBE_BUTTON_L)
		g_gamepad.l2 = true;
	else if(mapping == MAP_GAMECUBE_AXIS_X)
		g_gamepad.lx = value;
	else if(mapping == MAP_GAMECUBE_AXIS_Y)
		g_gamepad.ly = value;
	else if(mapping == MAP_GAMECUBE_AXIS_CX)
		g_gamepad.rx = value;
	else if(mapping == MAP_GAMECUBE_AXIS_CY)
		g_gamepad.ry = value;
	else if(mapping == MAP_GAMECUBE_AXIS_L)
		g_gamepad.al = value;
	else if(mapping == MAP_GAMECUBE_AXIS_R)
		g_gamepad.ar = value;
}

#ifdef WIN32
int main()
#else
int tests_main()
#endif
{
	// uint16 -> uint16 (passthrough)
	assert(0x0000 == convert_range(0x0000, 0x0000, 0xFFFF, VALUE_TYPE_UINT16));
	assert(0x4000 == convert_range(0x4000, 0x0000, 0xFFFF, VALUE_TYPE_UINT16));
	assert(0x8000 == convert_range(0x8000, 0x0000, 0xFFFF, VALUE_TYPE_UINT16));
	assert(0xC000 == convert_range(0xC000, 0x0000, 0xFFFF, VALUE_TYPE_UINT16));
	assert(0xFFFF == convert_range(0xFFFF, 0x0000, 0xFFFF, VALUE_TYPE_UINT16));

	// int16 -> int16 (passthrough)
	assert(-32768 == convert_range(-32768, INT16_MIN, INT16_MAX, VALUE_TYPE_INT16));
	assert(-16384 == convert_range(-16384, INT16_MIN, INT16_MAX, VALUE_TYPE_INT16));
	assert(     0 == convert_range(     0, INT16_MIN, INT16_MAX, VALUE_TYPE_INT16));
	assert( 16384 == convert_range( 16384, INT16_MIN, INT16_MAX, VALUE_TYPE_INT16));
	assert( 32767 == convert_range( 32767, INT16_MIN, INT16_MAX, VALUE_TYPE_INT16));

	// uint8 -> uint8 (passthrough)
	assert(0x00 == convert_range(0x00, 0, UINT8_MAX, VALUE_TYPE_UINT8));
	assert(0x40 == convert_range(0x40, 0, UINT8_MAX, VALUE_TYPE_UINT8));
	assert(0x80 == convert_range(0x80, 0, UINT8_MAX, VALUE_TYPE_UINT8));
	assert(0xC0 == convert_range(0xC0, 0, UINT8_MAX, VALUE_TYPE_UINT8));
	assert(0xFF == convert_range(0xFF, 0, UINT8_MAX, VALUE_TYPE_UINT8));

	// int8 -> int8 (passthrough)
	assert(-128 == convert_range(-128, INT8_MIN, INT8_MAX, VALUE_TYPE_INT8));
	assert( -64 == convert_range( -64, INT8_MIN, INT8_MAX, VALUE_TYPE_INT8));
	assert(   0 == convert_range(   0, INT8_MIN, INT8_MAX, VALUE_TYPE_INT8));
	assert(  64 == convert_range(  64, INT8_MIN, INT8_MAX, VALUE_TYPE_INT8));
	assert( 127 == convert_range( 127, INT8_MIN, INT8_MAX, VALUE_TYPE_INT8));

	// int8 -> uint8
	assert(0x00 == convert_range(-128, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT8));
	assert(0x40 == convert_range( -64, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT8));
	assert(0x80 == convert_range(   0, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT8));
	assert(0xC0 == convert_range(  64, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT8));
	assert(0xFF == convert_range( 127, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT8));

	// uint8 -> int8
	assert(-128 == convert_range(0x00, 0, UINT8_MAX, VALUE_TYPE_INT8));
	assert( -64 == convert_range(0x40, 0, UINT8_MAX, VALUE_TYPE_INT8));
	assert(   0 == convert_range(0x80, 0, UINT8_MAX, VALUE_TYPE_INT8));
	assert(  64 == convert_range(0xC0, 0, UINT8_MAX, VALUE_TYPE_INT8));
	assert( 127 == convert_range(0xFF, 0, UINT8_MAX, VALUE_TYPE_INT8));

	// uint8 -> uint16 (bit shift)
	assert(0x0000 == convert_range(0x00, 0, UINT8_MAX, VALUE_TYPE_UINT16));
	assert(0x4000 == convert_range(0x40, 0, UINT8_MAX, VALUE_TYPE_UINT16));
	assert(0x8000 == convert_range(0x80, 0, UINT8_MAX, VALUE_TYPE_UINT16));
	assert(0xC000 == convert_range(0xC0, 0, UINT8_MAX, VALUE_TYPE_UINT16));
	assert(0xFF00 == convert_range(0xFF, 0, UINT8_MAX, VALUE_TYPE_UINT16));

	// uint8 -> int16 (bit shift)
	assert(-32768 == convert_range(0x00, 0, UINT8_MAX, VALUE_TYPE_INT16));
	assert(-16384 == convert_range(0x40, 0, UINT8_MAX, VALUE_TYPE_INT16));
	assert(     0 == convert_range(0x80, 0, UINT8_MAX, VALUE_TYPE_INT16));
	assert( 16384 == convert_range(0xC0, 0, UINT8_MAX, VALUE_TYPE_INT16));
	assert(0x7F00 == convert_range(0xFF, 0, UINT8_MAX, VALUE_TYPE_INT16));

	// int8 -> uint16
	assert(0x0000 == convert_range(-128, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT16));
	assert(0x4000 == convert_range( -64, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT16));
	assert(0x8000 == convert_range(   0, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT16));
	assert(0xC000 == convert_range(  64, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT16));
	assert(0xFF00 == convert_range( 127, INT8_MIN, INT8_MAX, VALUE_TYPE_UINT16));

	// int8->int16 (bit shift)
	assert(-32768 == convert_range(-128, INT8_MIN, INT8_MAX, VALUE_TYPE_INT16));
	assert(-16384 == convert_range( -64, INT8_MIN, INT8_MAX, VALUE_TYPE_INT16));
	assert(     0 == convert_range(   0, INT8_MIN, INT8_MAX, VALUE_TYPE_INT16));
	assert( 16384 == convert_range(  64, INT8_MIN, INT8_MAX, VALUE_TYPE_INT16));
	assert(0x7F00 == convert_range( 127, INT8_MIN, INT8_MAX, VALUE_TYPE_INT16));

	// uint16 -> uint8 (bit shift >>)
	assert(0x00 == convert_range(0x0000, 0x0000, UINT16_MAX, VALUE_TYPE_UINT8));
	assert(0x40 == convert_range(0x4000, 0x0000, UINT16_MAX, VALUE_TYPE_UINT8));
	assert(0x80 == convert_range(0x8000, 0x0000, UINT16_MAX, VALUE_TYPE_UINT8));
	assert(0xC0 == convert_range(0xC000, 0x0000, UINT16_MAX, VALUE_TYPE_UINT8));
	assert(0xFF == convert_range(0xFFFF, 0x0000, UINT16_MAX, VALUE_TYPE_UINT8));

	// uint16 -> int8
	assert(-128 == convert_range(0x0000, 0x0000, UINT16_MAX, VALUE_TYPE_INT8));
	assert( -64 == convert_range(0x4000, 0x0000, UINT16_MAX, VALUE_TYPE_INT8));
	assert(   0 == convert_range(0x8000, 0x0000, UINT16_MAX, VALUE_TYPE_INT8));
	assert(  64 == convert_range(0xC000, 0x0000, UINT16_MAX, VALUE_TYPE_INT8));
	assert( 127 == convert_range(0xFFFF, 0x0000, UINT16_MAX, VALUE_TYPE_INT8));

	// int16 -> uint8
	assert(0x00 == convert_range(-32768, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT8));
	assert(0x40 == convert_range(-16384, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT8));
	assert(0x80 == convert_range(     0, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT8));
	assert(0xC0 == convert_range( 16384, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT8));
	assert(0xFF == convert_range( 32767, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT8));

	// int16 -> int8
	assert(-128 == convert_range(-32768, INT16_MIN, INT16_MAX, VALUE_TYPE_INT8));
	assert( -64 == convert_range(-16384, INT16_MIN, INT16_MAX, VALUE_TYPE_INT8));
	assert(   0 == convert_range(     0, INT16_MIN, INT16_MAX, VALUE_TYPE_INT8));
	assert(  64 == convert_range( 16384, INT16_MIN, INT16_MAX, VALUE_TYPE_INT8));
	assert( 127 == convert_range( 32767, INT16_MIN, INT16_MAX, VALUE_TYPE_INT8));

	// uint16 -> int16
	assert(-32768 == convert_range(0x0000, 0x0000, UINT16_MAX, VALUE_TYPE_INT16));
	assert(-16384 == convert_range(0x4000, 0x0000, UINT16_MAX, VALUE_TYPE_INT16));
	assert(     0 == convert_range(0x8000, 0x0000, UINT16_MAX, VALUE_TYPE_INT16));
	assert( 16384 == convert_range(0xC000, 0x0000, UINT16_MAX, VALUE_TYPE_INT16));
	assert( 32767 == convert_range(0xFFFF, 0x0000, UINT16_MAX, VALUE_TYPE_INT16));

	// int16 -> uint16
	assert(0x0000 == convert_range(-32768, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT16));
	assert(0x4000 == convert_range(-16384, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT16));
	assert(0x8000 == convert_range(     0, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT16));
	assert(0xC000 == convert_range( 16384, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT16));
	assert(0xFFFF == convert_range( 32767, INT16_MIN, INT16_MAX, VALUE_TYPE_UINT16));

	assert(0x80 == map_to_uint8((int32_t)   0, INT8_MIN, INT8_MAX));
	assert(0xC0 == map_to_uint8((int32_t)  64, INT8_MIN, INT8_MAX));
	assert(0x40 == map_to_uint8((int32_t) -64, INT8_MIN, INT8_MAX));
	assert(0x00 == map_to_uint8((int32_t)-128, INT8_MIN, INT8_MAX));
	assert(0xFF == map_to_uint8((int32_t) 127, INT8_MIN, INT8_MAX));

	assert(0x80 == map_to_uint8((int32_t)     0, INT16_MIN, INT16_MAX));
	assert(0xC0 == map_to_uint8((int32_t) 16384 + 64, INT16_MIN, INT16_MAX));
	assert(0x40 == map_to_uint8((int32_t)-16384, INT16_MIN, INT16_MAX));
	assert(0x00 == map_to_uint8((int32_t)-32768, INT16_MIN, INT16_MAX));
	assert(0xFF == map_to_uint8((int32_t) 32767, INT16_MIN, INT16_MAX));

	assert(0x80 == map_to_uint8(     0, INT16_MIN, INT16_MAX));
	assert(0xC0 == map_to_uint8( 16384 + 64, INT16_MIN, INT16_MAX));
	assert(0x40 == map_to_uint8(-16384, INT16_MIN, INT16_MAX));
	assert(0x00 == map_to_uint8(-32768, INT16_MIN, INT16_MAX));
	assert(0xFF == map_to_uint8( 32767, INT16_MIN, INT16_MAX));

	// Sony DualShock 4
	ParseReportDescriptor(dualshock4_hid_report_descriptor, sizeof(dualshock4_hid_report_descriptor), hid_to_gamecube_mapping);

	ParseReportDescriptor(my_dualshock_4_hid_report_descriptor, sizeof(my_dualshock_4_hid_report_descriptor), hid_to_gamecube_mapping);
	g_gamepad = {};
	ParseReport(my_dualshock_4_hid_report_x_o_pressed, sizeof(my_dualshock_4_hid_report_x_o_pressed), gamepad_callback);

	assert(g_gamepad.a);
	assert(g_gamepad.b);
	assert(g_gamepad.al == 0x00);
	assert(g_gamepad.ar == 0x00);
	assert(g_gamepad.lx == 0x7e);
	assert(g_gamepad.ly == 0x83);
	assert(g_gamepad.rx == 0x7e);
	assert(g_gamepad.ry == 0x7f);

	g_gamepad = {};
	ParseReport(my_dualshock_4_hid_report_u_x_pressed, sizeof(my_dualshock_4_hid_report_u_x_pressed), gamepad_callback);
	assert(g_gamepad.u);
	assert(g_gamepad.a);

	g_gamepad = {};
	ParseReport(my_dualshock_4_hid_report_options_r2_max_pressed, sizeof(my_dualshock_4_hid_report_options_r2_max_pressed), gamepad_callback);
	assert(g_gamepad.start);
	assert(g_gamepad.r2);
	assert(g_gamepad.ar == 0xFF);

	g_gamepad = {};
	ParseReport(my_dualshock_4_hid_report_lx_rx_min, sizeof(my_dualshock_4_hid_report_lx_rx_min), gamepad_callback);
	assert(g_gamepad.lx == 0x00);
	assert(g_gamepad.rx == 0x00);

	ParseReportDescriptor(dualshock_4_hid_report_descriptor_gimx_fr_wiki, sizeof(dualshock_4_hid_report_descriptor_gimx_fr_wiki), hid_to_gamecube_mapping);
	g_gamepad = {};
	ParseReport(dualshock_4_hid_report_gimx_fr_wiki, sizeof(dualshock_4_hid_report_gimx_fr_wiki), gamepad_callback);

	assert(g_gamepad.al == 0x00);
	assert(g_gamepad.ar == 0x00);
	assert(g_gamepad.lx == 0x81);
	assert(g_gamepad.ly == 0x80);
	assert(g_gamepad.rx == 0x83);
	assert(g_gamepad.ry == 0x7a);

	// Sony DualShock 3
	ParseReportDescriptor(dualshock_3_hid_report_descriptor, sizeof(dualshock_3_hid_report_descriptor), hid_to_gamecube_mapping);
	g_gamepad = {};

	// Sony PS5 DualSence
	ParseReportDescriptor(dualsence_hid_report_descriptor, sizeof(dualsence_hid_report_descriptor), hid_to_gamecube_mapping);
	g_gamepad = {};
	ParseReport(dualsence_hid_report_idle, sizeof(dualsence_hid_report_idle), gamepad_callback);

	g_gamepad = {};
	ParseReport(dualsence_hid_report_x_o_pressed, sizeof(dualsence_hid_report_x_o_pressed), gamepad_callback);
	assert(g_gamepad.a);
	assert(g_gamepad.b);

	g_gamepad = {};
	ParseReport(dualsence_hid_report_u_x_pressed, sizeof(dualsence_hid_report_u_x_pressed), gamepad_callback);
	assert(g_gamepad.u);
	assert(g_gamepad.a);

	g_gamepad = {};
	ParseReport(dualsence_hid_report_options_r2_max_pressed, sizeof(dualsence_hid_report_options_r2_max_pressed), gamepad_callback);
	assert(g_gamepad.start);
	assert(g_gamepad.r2);
	assert(g_gamepad.ar == 0xFF);

	g_gamepad = {};
	ParseReport(dualsence_hid_report_lx_rx_min, sizeof(dualsence_hid_report_lx_rx_min), gamepad_callback);
	assert(g_gamepad.lx == 0x08);
	assert(g_gamepad.rx == 0x01);

	// HID keyboard
	ParseReportDescriptor(keyboard_report_descriptor, sizeof(keyboard_report_descriptor), nullptr);
	g_keyboard = {};

	ParseReport(keyboard_report_a_pressed, sizeof(keyboard_report_a_pressed), nullptr, keyboard_callback);
	assert(g_keyboard.keys[0x04] == true);

	ParseReport(keyboard_report_none_pressed, sizeof(keyboard_report_none_pressed), nullptr, keyboard_callback);
	assert(g_keyboard.keys[0x04] == false);

	// HID mouse
	ParseReportDescriptor(mouse_report_descriptor, sizeof(mouse_report_descriptor), nullptr);
	g_mouse = {};

	ParseReport(mouse_report_1, sizeof(mouse_report_1), nullptr, nullptr, mouse_callback);
	assert(g_mouse.buttons == 0x00);
	assert(g_mouse.x == g_mouse.y == g_mouse.z == 0x0000);

	ParseReport(mouse_report_2, sizeof(mouse_report_2), nullptr, nullptr, mouse_callback);
	assert(g_mouse.buttons & 0x01);

	ParseReport(mouse_report_3, sizeof(mouse_report_3), nullptr, nullptr, mouse_callback);
	assert(g_mouse.buttons & 0x02);

	ParseReport(mouse_report_4, sizeof(mouse_report_4), nullptr, nullptr, mouse_callback);
	assert(g_mouse.buttons == 0x00);
	assert(g_mouse.z == -1);

	// HID gamepad to keyboard mapping
	const uint8_t HID_KEY_A = 0x04;

	const JoyPreset gamepad_to_keyboard_mapping[] =
	{
		// Map gamepad button 2 to keyboard A key
		{ 1, REPORT_USAGE_PAGE_BUTTON, 2, MAP_KEYBOARD, HID_KEY_A, MAP_TYPE_THRESHOLD_ABOVE, 0 },
		// null record to mark end
		{ 0, 0, 0, 0, 0, 0, 0 }
	};

	ParseReportDescriptor(my_dualshock_4_hid_report_descriptor, sizeof(my_dualshock_4_hid_report_descriptor), gamepad_to_keyboard_mapping);

	ParseReport(my_dualshock_4_hid_report_u_x_pressed, sizeof(my_dualshock_4_hid_report_u_x_pressed), gamepad_callback, keyboard_callback);
	assert(g_keyboard.keys[HID_KEY_A] == true);

	ParseReport(my_dualshock_4_hid_report_idle, sizeof(my_dualshock_4_hid_report_idle), gamepad_callback, keyboard_callback);
	assert(g_keyboard.keys[HID_KEY_A] == false);

	return 0;
}