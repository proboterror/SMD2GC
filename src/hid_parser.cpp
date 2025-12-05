#include <new>
#include <cassert>
#include <cstring>
#include <stdint.h>
#include <stdio.h>

#include "hid_parser.h"

#include "arena_allocator.h"
#include "hid_dumps.h"

#define LITTLE_ENDIAN 0 // RP2040 is little endian / Intel x86 is big endian

/*
Reference:
USB Device Class Definition for Human Interface Devices (HID)
Version 1.11
https://www.usb.org/sites/default/files/documents/hid1_11.pdf

5.3 Generic Item Format
An item is piece of information about the device. All items have a one-byte prefix
that contains the item tag, item type, and item size.

Bits   23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8  7 6 5 4  3 2   1 0
       ┌──────────────────────┐┌────────────────────┐┌───────┬─────┬─────┐
Parts  │        [data]        ││       [data]       ││ bTag  │bType│bSize│
       └──────────────────────┘└────────────────────┘└───────┴─────┴─────┘
Bytes             2                       1                   0

There are two basic types of items: short　items and long items.
If the item is a short item, its optional data size may be 0, 1,　2, or 4 bytes.

5.4 Item Parser
From the parser’s point of view, a HID class device looks like the following　figure:

                                    ┌────────────────────┐
                                    │    Application     │
                                    │    Collection      │
                                    └────────────────────┘
                                              │
                             ┌────────────────┴──────────────────────┐
                             ▼                                       ▼
                    ┌────────────────┐                       ┌────────────────┐
                    │   Collection   │                       │     Report     │
                    └────────────────┘                       └────────────────┘
                             │                                       │
                   ┌─────────┴──────────┐                            │
                   ▼                    ▼                            ▼
           ┌──────────────┐    ┌──────────────┐          ┌────────────────────────┐
           │    Report    │    │    Report    │          │ Main Item              │
           └──────────────┘    └──────────────┘          │ Report Size            │
                   │                    │                │ Report Count           │
                   ▼                    ▼                └────────────────────────┘
        ┌─────────────────────┐ ┌─────────────────────┐              │
        │ Main Item           │ │ Main Item           │       ┌──────┴───────┐
        │ Report Size         │ │ Report Size         │       │              │
        │ Report Count        │ │ Report Count        │       ▼              ▼
        │ Logical Minimum     │ │ Logical Minimum     │  ┌─────────┐    ┌─────────┐
        │ Logical Maximum     │ │ Logical Maximum     │  │  Usage  │    │  Usage  │
        └─────────────────────┘ └─────────────────────┘  └─────────┘    └─────────┘
                   │                       │
         ┌─────────┴─────────┐             │
         ▼         ▼         ▼             ▼
     ┌───────┐ ┌───────┐ ┌───────┐     ┌───────┐
     │ Usage │ │ Usage │ │ Usage │     │ Usage │
     └───────┘ └───────┘ └───────┘     └───────┘

When a Main item is found, a new report structure is allocated and initialized
with the current item state table. All Local items are then removed from the
item state table, but Global items remain. In this way, Global items set the
default value for subsequent new Main items. A device with several similar
controls—for example, six axes—would need to define the Global items only
once prior to the first Main item.

Note: Main items are associated with a collection by the order in which they
are declared. A new collection starts when the parser reaches a Collection
item. The item parser associates with a collection all Main items defined
between the Collection item and the next End Collection item.

- When a Push item is encountered, the item state table is copied and placed on
a stack for later retrieval.

- When a Pop item is found, the item state table is replaced with the top table
from the stack.


There are three item types: Main, Global, and Local.

There are five Main item tags currently defined:
Input item tag:
	Refers to the data from one or more similar controls on a device.
	For example, variable data such as reading the position of a single axis
	or a group of levers or array data such as one or more push buttons or switches.
Output item tag:
	Refers to the data to one or more similar controls on a device
	such as setting the position of a single axis or a group of levers (variable data).
	Or, it can represent data to one or more LEDs (array data).
- Feature item tag:
	Describes device input and output not intended for consumption by the end user.
	For example, a software feature or Control Panel toggle.
- Collection item tag:
	A meaningful grouping of Input, Output, and Feature items.
	For example, mouse, keyboard, joystick, and pointer.
- End Collection item tag:
	A terminating item used to specify the end of a collection of items.

Main Items are the ones that actually declare real data fields (Input, Output, Feature) or structure the descriptor (Collection, End Collection).
They consume the current global item state (Report Size, Report Count, Logical Min/Max, Usage Page, etc.) and generate the actual report fields at that moment.

Complete list of all Main Items from the official USB HID 1.11/1.12 specification:
----------------------------------------------------------------------------------------------------
Tag (hex)	Item Name	Data Size		Meaning & Flags (bits 0–9)					Common Usage
----------------------------------------------------------------------------------------------------
0x80	Input		1, 2, or 4 bytes	Defines an input report field 				Mouse/buttons, keyboard keys, joystick axes, sensor values
										Bit 0: Constant (0) vs Data (1)
										Bit 1: Variable (0) vs Array (1)
										Bit 2: Absolute (0) vs Relative (1)
										Bit 3: No Wrap (0) vs Wrap (1)
										Bit 4: Linear (0) vs Non-Linear (1)
										Bit 5: Preferred State (0) vs No Preferred (1)
										Bit 6: No Null Position (0) vs Null State (1)
										Bit 7: Reserved (was Non-Volatile)
										Bit 8: Bit Field (0) vs Buffered Bytes (1)
										Bit 9–31: Reserved

0x90	Output		1, 2, or 4 bytes	Defines an output report field (e.g. LEDs, rumble)
										Same flag bits as Input						Keyboard LEDs, force-feedback, display backlight

0xB0	Feature		1, 2, or 4 bytes	Bi-directional field, used for configuration
										Same flag bits as Input/Output				DPI setting, battery level, calibration data, vendor config

0xA0	Collection	1 byte				Begins a collection							Almost always 0x01 (Application) for top-level
										0x00 Physical (group of axes)
										0x01 Application (mouse, keyboard)
										0x02 Logical (interrelated data)
										0x03 Report
										0x04 Named Array
										0x05 Usage Switch
										0x06 Usage Modifier
										0x80–0xFF Vendor-defined

0xC0	End Collection	0 bytes			Closes the most recent Collection			Mandatory pair with every Collection
----------------------------------------------------------------------------------------------------

Most Commonly Used Flag Combinations
-----------------------------------------------------------
Name						Hex		Binary flags	Meaning
									(bits 0–8)
-----------------------------------------------------------
Data, Variable, Absolute	0x02	000000010		Normal controls (mouse X/Y, analog triggers)
Data, Array, Absolute		0x00	000000000		Keyboard key arrays, button pages
Constant, Variable, Abs		0x03	000000011		Padding bits
Constant, Array, Abs		0x01	000000001		Padding bytes (reserved byte in keyboard report)
Data, Variable, Relative	0x06	000001100		Mouse wheel, relative axes
Data, Variable, Abs,
NoWrap, Linear, NoNull		0x02 (most common)		Standard absolute axis
-----------------------------------------------------------

Global items affect the interpretation of all subsequent items in the report descriptor until a new global item of the same type overrides it.

Complete list of all Global Items defined in the USB HID Device Class Definition (HID 1.11 and later), with their tags, data size, and meaning:
------------------------------------------------------------------------------------------------------------------------------
Tag (hex)	Item Name			Data Size	Description													Typical Values / Notes
------------------------------------------------------------------------------------------------------------------------------
0x04		Usage Page			1, 2, 4		Defines the usage page for subsequent Usage items			0x0001 (Generic Desktop), 0x0007 (Keyboard), 0x0009 (Button), 0xFF00 (Vendor-defined), etc.
0x14		Logical Minimum		1, 2, 4		Minimum value for logical (signed) data						e.g., -127, 0, -1
0x24		Logical Maximum		1, 2, 4		Maximum value for logical (signed) data						e.g., 127, 1, 255
0x34		Physical Minimum	1, 2, 4		Minimum value for physical data (after applying scale/bias)	Rarely used
0x44		Physical Maximum	1, 2, 4		Maximum value for physical data								Rarely used
0x54		Unit Exponent		1			Signed exponent for units (base-10)							-8 to +7 (e.g., 0x0F = -1 → divide by 10)
0x64		Unit				1, 2, 4		Defines the unit and system (SI, English, etc.)				e.g., 0x00000001 (None), 0xF0D12131 (degrees Celsius)
0x74		Report Size			1, 2, 4		Size of each field in bits									1, 8, 16, 32 most common
0x84		Report ID			1			Prefixes reports with an 8-bit ID (0 = no ID)				1–255; required if multiple report formats
0x94		Report Count		1, 2, 4		Number of fields of Report Size that follow					e.g., 6 for 6 keys in a keyboard report
0xA4		Push				0			Pushes current global state stack onto stack				Used for saving state
0xB4		Pop					0			Restores global state from top of stack						Used after Push
------------------------------------------------------------------------------------------------------------------------------

Quick Reference Table (most commonly used)
---------------------------------------------------------------------
Item				Tag		Typical Data Values		When to change it
---------------------------------------------------------------------
Usage Page			4		0x01, 0x07, 0x08,		When switching between keyboard, mouse, consumer, vendor
							0x09, 0x0C, 0xFF00
Logical Minimum		14		0, -1, -127				For signed values or when 0 isn't the minimum
Logical Maximum		24		1, 255, 32767			Almost always needed
Report Size			74		1, 8, 16, 32			Every time field size changes
Report Count		94		1, 3, 6, 8, etc.		Every time number of repeated fields changes
Report ID			84		1–255					Only if you have multiple report formats
---------------------------------------------------------------------

Local items apply only to the very next Main item (Input, Output, or Feature) that follows them.
After that Main item is processed, all local items are cleared and reset to “undefined” again.

Here is the complete list of all Local Items defined in the USB HID 1.11/1.12 specification:
-------------------------------------------------------------------------
Tag (hex)	Item Name	Data Size		Meaning	Typical Values / Examples
-------------------------------------------------------------------------
0x08	Usage				1, 2, 4		Identifies what the upcoming field actually is	0x30 (X), 0x31 (Y), 0x04 (Keyboard a/A), 0x07 (Battery Strength)
0x18	Usage Minimum		1, 2, 4		Lowest Usage value in a range (used with arrays or delimited sets)	0xE0 (Left Control), 0x01 (Button 1)
0x28	Usage Maximum		1, 2, 4		Highest Usage value in a range	0xE7 (Right GUI), 0x08 (Button 8)
0x38	Designator Index	1, 2, 4		Physical identifier (which finger, which hand, etc.) – rarely used	0x01 (First finger)
0x48	Designator Minimum	1, 2, 4		Start of designator range	—
0x58	Designator Maximum	1, 2, 4		End of designator range	—
0x68	String Index		1, 2, 4		Index into the string descriptor table (for human-readable names)	1 = first string, etc.
0x78	String Minimum		1, 2, 4		Start of string index range	—
0x88	String Maximum		1, 2, 4		End of string index range	—
0x98	Delimiter			1			Used to create multiple logical collections with the same local items (rare)	0x01 = open set, 0x00 = close set
-------------------------------------------------------------------------

How Local Items Are Consumed
Local items → Main item → Local items are discarded

Example:
0x05, 0x01,        // Usage Page (Generic Desktop)          ← Global
0x09, 0x30,        // Usage (X)                             ← Local
0x09, 0x31,        // Usage (Y)                             ← Local  ← still active
0x81, 0x02,        // Input (Data,Var,Abs)                  ← Main consumes both Usages → now local state empty again

The Two Most Important Patterns:

1. Variable fields (one Usage per field) – most common for axes, triggers, etc.
0x09, 0x30,        // Usage (X)                ← Local
0x09, 0x31,        // Usage (Y)                ← Local
0x09, 0x32,        // Usage (Z)                ← Local
0x95, 0x03,        // Report Count (3)         ← Global
0x81, 0x02,        // Input (Data,Var,Abs)     ← Main → three separate fields: X, Y, Z

2. Array fields (range of Usages) – used for keyboards, button pages, consumer keys
0x19, 0x01,        // Usage Minimum (Button 1)          ← Local
0x29, 0x0C,        // Usage Maximum (Button 12)         ← Local
0x95, 0x0C,        // Report Count (12)                 ← Global
0x75, 0x08,        // Report Size (8)                   ← Global
0x81, 0x00,        // Input (Data,Array,Abs)            ← Main → 12-byte array of button indices

Note:
Collection and End Collection are Main items that do not declare report data, therefore they do not consume local items.

The only thing that can give a meaning to a Collection is a Usage that appears immediately before the Collection (still a local item),
but the parser treats it specially: it is moved from the local state table into the collection’s own state before the local table is cleared.

0x09, 0x02,        // Usage (Mouse)                 ← Local Usage
0xA1, 0x01,        // Collection (Application)      ← This Collection now "owns" the Usage "Mouse"
*/

// Extend 2's complement number sign
#define SIGNEX(v, sb) ((v) | (((v) & (1 << (sb))) ? ~((1 << (sb))-1) : 0))

// Custom values for storing HID item format
enum hid_item_format
{
	HID_ITEM_FORMAT_SHORT,
	HID_ITEM_FORMAT_LONG
};

typedef struct _HID_ITEM_INFO
{
	uint8_t format; // hid_item_format
	uint8_t size;
	uint8_t type; // hid_item_type: MAIN, GLOBAL, LOCAL
	uint8_t tag; // hid_main_item_tag

	union
	{
		uint8_t  u8;
		int8_t   s8;
		uint16_t u16;
		int16_t  s16;
		uint32_t u32;
		int32_t  s32;

		const uint8_t* longdata;
	} value;

} HID_ITEM;

static uint16_t GetUnaligned16(const uint8_t* start)
{
#if LITTLE_ENDIAN
	return *(uint16_t*)start;
#else
	uint16_t dat = (*start) | (*(start + 1) << 8);

	return dat;
#endif
}

static uint32_t GetUnaligned32(const uint8_t* start)
{
#if LITTLE_ENDIAN
	return *(uint32_t*)start;
#else
	uint32_t dat = (uint32_t)(*start) | ((uint32_t)(*(start + 1)) << 8) | ((uint32_t)(*(start + 2)) << 16) | ((uint32_t)(*(start + 3)) << 24);

	return dat;
#endif
}

static uint32_t ItemUData(HID_ITEM* itemInfo)
{
	switch (itemInfo->size)
	{
	case 1:
		return itemInfo->value.u8;
	case 2:
		return itemInfo->value.u16;
	case 4:
		return itemInfo->value.u32;
	}

	return 0;
}

static int32_t ItemSData(HID_ITEM* itemInfo)
{
	switch (itemInfo->size)
	{
	case 1:
		return SIGNEX(itemInfo->value.s8, 7);
	case 2:
		return SIGNEX(itemInfo->value.s16, 15);
	case 4:
		return itemInfo->value.s32;
	}

	return 0;
}

/*
  Parsing introduction:
  https://docs.kernel.org/hid/hidintro.html
  https://docs.kernel.org/hid/hidreport-parsing.html
  https://www.usb.org/sites/default/files/documents/hid1_11.pdf
*/
// Returns pointer to next item
const uint8_t* fetch_item(const uint8_t* start, const uint8_t* end, HID_ITEM* item)
{
/*
Bits   23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8  7 6 5 4  3 2   1 0
       ┌──────────────────────┐┌────────────────────┐┌───────┬─────┬─────┐
Parts  │        [data]        ││       [data]       ││ bTag  │bType│bSize│
       └──────────────────────┘└────────────────────┘└───────┴─────┴─────┘
Bytes             2                       1                   0

Item firs byte:
	2 bits for the length of the item,
	2 bits for the type of the item and
	4 bits for the tag / function
*/
	if (end - start <= 0)
		return nullptr;

	const uint8_t first = *start++;

	item->type = (first >> 2) & 0x03; // 2 bits for the type

	item->tag = (first >> 4) & 0x0F; // 4 bits for function

	if (item->tag == HID_ITEM_TAG_LONG) // Long items - HID 1.11 section 6.2.2.3
	{
		item->format = HID_ITEM_FORMAT_LONG;

		if (end - start < 2)
			return nullptr;

		item->size = *start++;
		item->tag = *start++;

		if (end - start < item->size)
			return nullptr;

		item->value.longdata = start;
		start += item->size;

		return start;
	}
	else
	{
		item->format = HID_ITEM_FORMAT_SHORT; // Short Items - HID 1.11 section 6.2.2.2
		item->size = first & 0x03; // 2 bits for size

		switch (item->size)
		{
		case 0:
			return start;

		case 1:
			if (end - start < 1)
				return nullptr;

			item->value.u8 = *start++;

			return start;

		case 2:
			if (end - start < 2)
				return nullptr;

			item->value.u16 = GetUnaligned16(start);
			start += 2;

			return start;

		case 3:
			item->size++;

			if (end - start < 4)
				return nullptr;

			item->value.u32 = GetUnaligned32(start);
			start += 4;

			return start;
		}
	}

	return nullptr;
}

// defines a mapping between a HID segment and an output event
typedef struct HID_SEG
{
	uint16_t startBit;

	// Mouse or keyboard or Kempston joystick
	uint8_t outputChannel;

	// for keyboard, this is the HID scancode of the key associated with this control
	// for mouse, this is one of the values of MAP_MOUSE_x
	// for gamepad, this is target mapped value (target gamepad button / axis)
	uint8_t outputControl;

	// How this control gets interpreted - MAP_TYPE_x
	uint8_t inputType;

	// Param has different meanings depending on InputType
	uint16_t inputParam;

	// Assume HID item size does not exceed 16 bit for X/Y axes to save embedded memory and CPU cycles.
	// However, by specification it can be 32 bit.
	int16_t logicalMinimum;
	uint16_t logicalMaximum;

	uint8_t reportSize; // Item size in bits

	uint8_t reportCount; // Used for bitfields / arrays

	uint32_t value; // User value used in mapping function

	struct HID_SEG* next;
} HID_SEG;

#define KEYBOARD_STATE_SIZE 256/8 // bit map for currently pressed keys (0-256)

typedef struct keyboard_state
{
	uint8_t keys[KEYBOARD_STATE_SIZE];
	uint8_t old_keys[KEYBOARD_STATE_SIZE];
} keyboard_state;

typedef struct _HID_REPORT
{
	uint8_t reportID;

	uint16_t appUsage; // generic_page_input_usage
	uint16_t appUsagePage; // hid_usage_pages
	uint16_t length; // in bits

	keyboard_state keyboard;

	HID_SEG* segments;

	struct _HID_REPORT* next;
} HID_REPORT;

bool g_interface_uses_reports = false;
HID_REPORT* g_reports = nullptr;

typedef struct _HID_GLOBAL
{
	uint16_t usagePage; // Up to 4 bytes by spec
	int32_t logicalMinimum;
	uint32_t logicalMaximum;
	int16_t physicalMinimum; // Up to 4 bytes by spec. Ignored.
	uint16_t physicalMaximum;
	uint8_t reportID;
	uint8_t reportSize; // Current item size in bits
	uint8_t reportCount; // Current item reports count
} HID_GLOBAL;

typedef struct _HID_LOCAL
{
	uint32_t usage;
	uint32_t usageMin;
	uint32_t usageMax;
} HID_LOCAL;

// There is no fixed limit in the HID specification.
// Most parsers limit to 255 usages per MAIN item.
// In practice do not exceed 10.
#define MAX_USAGE_NUM 16

typedef struct ParseState
{
	HID_GLOBAL hidGlobal;
	HID_LOCAL hidLocal;
	uint16_t startBit;
	uint16_t appUsage; // Stored LOCAL Usage for Collection (Application) (generic_page_input_usage)
	uint16_t appUsagePage; // Stored GLOBAL Usage Page for Collection (Application) (hid_usage_pages)
	uint8_t joyNum;
	uint8_t usages[MAX_USAGE_NUM];
	uint8_t usagesCount;
} ParseState;

ParseState g_HIDParseState = {};

void hid_parser_reset_state()
{
	arena_reset();

	g_interface_uses_reports = false;
	g_reports = nullptr;
	g_HIDParseState = {};
}

HID_SEG* CreateSeg(HID_REPORT* rep, const uint16_t startbit)
{
	HID_SEG* segment = new(arena_alloc(sizeof(HID_SEG))) HID_SEG{};

	segment->next = rep->segments;
	rep->segments = segment;

	segment->startBit = startbit;
	segment->reportCount = g_HIDParseState.hidGlobal.reportCount;
	segment->reportSize = g_HIDParseState.hidGlobal.reportSize;

	if (segment->reportSize > 16)
	{
		printf("\nWarning: CreateSeg: 32-bit HID value");
	}

	segment->logicalMinimum = (int16_t)g_HIDParseState.hidGlobal.logicalMinimum; // reduce range from 32 to 16 bit
	segment->logicalMaximum = (uint16_t)g_HIDParseState.hidGlobal.logicalMaximum;

	return segment;
}

//search though preset to see if this matches a mapping
void CreateMapping(HID_REPORT* rep, const JoyPreset* preset, const uint16_t startbit)
{
	// Empty structure used as end of JoyPreset array.
	// Can be used with more safe array indexing, with end marker deleted.
	while (preset->inputType != MAP_TYPE_NONE)
	{
		if (preset->inputUsagePage == g_HIDParseState.hidGlobal.usagePage &&
			preset->inputUsage == g_HIDParseState.hidLocal.usage &&
			preset->number == g_HIDParseState.joyNum)
		{
			HID_SEG* segment = CreateSeg(rep, startbit);
			segment->outputChannel = preset->outputChannel;
			segment->outputControl = preset->outputControl;
			segment->inputType = preset->inputType;
			segment->inputParam = preset->inputParam;
		}

		preset++;
	}
}

void CreateBitfieldMapping(HID_REPORT* rep, const JoyPreset* preset)
{
	uint16_t startbit = g_HIDParseState.startBit;

	if (g_HIDParseState.appUsagePage == REPORT_USAGE_PAGE_GENERIC_DESKTOP)
	{
		if (g_HIDParseState.appUsage == REPORT_USAGE_KEYBOARD)
		{
			if (g_HIDParseState.hidGlobal.usagePage == REPORT_USAGE_PAGE_KEYBOARD)
			{
				HID_SEG* segment = CreateSeg(rep, startbit);
				// Keyboard - 1 bit per key (usually for modifier field)
				segment->outputChannel = MAP_KEYBOARD;
				segment->outputControl = g_HIDParseState.hidLocal.usageMin;
				segment->inputType = MAP_TYPE_BITFIELD;
			}
		}
		else if (g_HIDParseState.appUsage == REPORT_USAGE_MOUSE)
		{
			if (g_HIDParseState.hidGlobal.usagePage == REPORT_USAGE_PAGE_BUTTON)
			{
				HID_SEG* segment = CreateSeg(rep, startbit);
				// Mouse - 1 bit per button
				segment->outputChannel = MAP_MOUSE;
				segment->outputControl = g_HIDParseState.hidLocal.usageMin;
				segment->inputType = MAP_TYPE_BITFIELD;
			}
		}
		else if (g_HIDParseState.appUsage == REPORT_USAGE_JOYSTICK || g_HIDParseState.appUsage == REPORT_USAGE_GAMEPAD)
		{
			for (uint8_t i = g_HIDParseState.hidLocal.usageMin; i < g_HIDParseState.hidLocal.usageMax; i++)
			{
				g_HIDParseState.hidLocal.usage = i; // used as global variable for parameter passing
				CreateMapping(rep, preset, startbit);

				startbit += g_HIDParseState.hidGlobal.reportSize;
			}
		}
	}
}

void CreateUsageMapping(HID_REPORT* rep, const JoyPreset* preset)
{
	uint16_t startbit = g_HIDParseState.startBit;

	// need to make a seg for each found usage
	for (uint8_t i = 0; i < g_HIDParseState.usagesCount; i++)
	{
		if (g_HIDParseState.appUsagePage == REPORT_USAGE_PAGE_GENERIC_DESKTOP)
		{
			if (g_HIDParseState.appUsage == REPORT_USAGE_MOUSE)
			{
				HID_SEG* segment = CreateSeg(rep, startbit);

				startbit += g_HIDParseState.hidGlobal.reportSize;

				if (g_HIDParseState.hidGlobal.usagePage == REPORT_USAGE_PAGE_GENERIC_DESKTOP)
				{
					segment->outputChannel = MAP_MOUSE;
					switch (g_HIDParseState.usages[i])
					{
					case REPORT_USAGE_X:
						// Mouse - value field
						segment->outputControl = MAP_MOUSE_X;
						segment->inputType = MAP_TYPE_SCALE;
						break;

					case REPORT_USAGE_Y:
						// Mouse - value field
						segment->outputControl = MAP_MOUSE_Y;
						segment->inputType = MAP_TYPE_SCALE;
						break;

					case REPORT_USAGE_WHEEL:
						// Mouse - value field
						segment->outputControl = MAP_MOUSE_WHEEL;
						segment->inputType = MAP_TYPE_SCALE;
						break;
					}
				}
			}
			else if (g_HIDParseState.appUsage == REPORT_USAGE_JOYSTICK || g_HIDParseState.appUsage == REPORT_USAGE_GAMEPAD)
			{
				g_HIDParseState.hidLocal.usage = g_HIDParseState.usages[i]; // used as global variable for parameter passing
				CreateMapping(rep, preset, startbit);

				startbit += g_HIDParseState.hidGlobal.reportSize;
			}
		}
	}
}

void CreateArrayMapping(HID_REPORT* rep)
{
	uint16_t startbit = g_HIDParseState.startBit;

	if (g_HIDParseState.appUsagePage == REPORT_USAGE_PAGE_GENERIC_DESKTOP &&
		g_HIDParseState.appUsage == REPORT_USAGE_KEYBOARD &&
		g_HIDParseState.hidGlobal.usagePage == REPORT_USAGE_PAGE_KEYBOARD)
	{
		// need to make a seg for each report array item
		for (uint8_t i = 0; i < g_HIDParseState.hidGlobal.reportCount; i++)
		{
			HID_SEG* segment = CreateSeg(rep, startbit);
			segment->outputChannel = MAP_KEYBOARD;
			segment->inputType = MAP_TYPE_ARRAY;

			startbit += g_HIDParseState.hidGlobal.reportSize;
		}
	}
}

void DumpHID(HID_REPORT* report)
{
	while (report)
	{
		HID_SEG* seg = report->segments;

		printf("Report: usage %x, length %u: \n",  report->appUsage, report->length);

		while (seg)
		{
			printf("startBit %u, inputType %hx, inputParam %x, outputChannel %hx, outputControl %hx, size %hx, count %hx\n",
				seg->startBit, seg->inputType, seg->inputParam, 
				seg->outputChannel, seg->outputControl,
				seg->reportSize, seg->reportCount);

			seg = seg->next;
		}

		report = report->next;
	}
}

bool ParseReportDescriptor(const uint8_t* descriptor, const uint16_t len, const JoyPreset* preset)
{
	hid_parser_reset_state();

	HID_GLOBAL* hidGlobal = &g_HIDParseState.hidGlobal;
	HID_LOCAL* hidLocal = &g_HIDParseState.hidLocal;

	hidLocal->usageMax = 0xFFFF;
	hidLocal->usageMin = 0xFFFF;

	HID_ITEM item;

	HID_REPORT* currHidReport = nullptr;

	uint8_t collectionDepth = 0;

	const uint8_t* start = descriptor;
	const uint8_t* end = start + len;

	while ((start = fetch_item(start, end, &item)))
	{
		if (item.format != HID_ITEM_FORMAT_SHORT)
		{
			return false; // Long HID items not supported.
		}

		switch (item.type)
		{
		case HID_TYPE_MAIN:
			if (item.tag == HID_MAIN_ITEM_TAG_INPUT)
			{
				if (
					g_HIDParseState.appUsagePage == REPORT_USAGE_PAGE_GENERIC_DESKTOP &&
						(
						g_HIDParseState.appUsage == REPORT_USAGE_JOYSTICK ||
						g_HIDParseState.appUsage == REPORT_USAGE_GAMEPAD ||
						g_HIDParseState.appUsage == REPORT_USAGE_KEYBOARD ||
						g_HIDParseState.appUsage == REPORT_USAGE_MOUSE
						)
					)
				{
					if (currHidReport == nullptr)
					{
						// Start new report within descriptor
						currHidReport = new(arena_alloc(sizeof(HID_REPORT))) HID_REPORT{};
						currHidReport->reportID = hidGlobal->reportID;

						currHidReport->next = g_reports; // Add new report to list head. Note: For report parsing lookup better to add to list tail.
						g_reports = currHidReport;

						currHidReport->appUsagePage = g_HIDParseState.appUsagePage;
						currHidReport->appUsage = g_HIDParseState.appUsage;

						if ((g_HIDParseState.appUsage == REPORT_USAGE_JOYSTICK || g_HIDParseState.appUsage == REPORT_USAGE_GAMEPAD))
							g_HIDParseState.joyNum++;
					}

					if (ItemUData(&item) & HID_INPUT_VARIABLE)
					{
						// we found some discrete usages, get to it
						if (g_HIDParseState.usagesCount)
						{
							CreateUsageMapping(currHidReport, preset);
						}
						// if no usages found, maybe a bitfield
						else if (hidLocal->usageMin != 0xFFFF && hidLocal->usageMax != 0xFFFF &&
							hidGlobal->reportSize == 1)
						{
							CreateBitfieldMapping(currHidReport, preset);
						}
						else
						{
							// Input Variable MAIN item with no usages / usage min/max declared.
						}
					}
					else // Item is array style, whole range appears in every segment
					{
						CreateArrayMapping(currHidReport);
					}
				}

				g_HIDParseState.startBit += (uint16_t)hidGlobal->reportSize * (uint16_t)hidGlobal->reportCount;

				if (currHidReport)
					currHidReport->length = g_HIDParseState.startBit;
			}
			else if (item.tag == HID_MAIN_ITEM_TAG_COLLECTION_START)
			{
				collectionDepth++;

				if (ItemUData(&item) == HID_COLLECTION_APPLICATION_)
				{
					// Make a note of this application collection's usage/page
					// (so we know what sort of device this is)
					g_HIDParseState.appUsage = hidLocal->usage;

					g_HIDParseState.appUsagePage = hidGlobal->usagePage;
				}
			}
			else if (item.tag == HID_MAIN_ITEM_TAG_COLLECTION_END)
			{
				collectionDepth--;

				// Only advance app if we're at the root level
				if (collectionDepth == 0)
				{
					g_HIDParseState.appUsage = 0x00;
					g_HIDParseState.appUsagePage = 0x00;
				}
			}

			// Output and Feature MAIN items are ignored.

			g_HIDParseState.usagesCount = 0; // reset parser state / LOCAL Usage list after MAIN Input item 
			// Local items → Main item → Local items are discarded
			hidLocal->usage = 0x00;
			hidLocal->usageMax = 0xFFFF;
			hidLocal->usageMin = 0xFFFF;
			break;

		case HID_TYPE_GLOBAL:
			switch (item.tag)
			{
			case HID_GLOBAL_ITEM_TAG_REPORT_ID:
				g_interface_uses_reports = true;
				// report id
				g_HIDParseState.startBit = 0;
				g_HIDParseState.startBit += item.size * 8; // Report starts with report ID

				hidGlobal->reportID = ItemUData(&item);
				currHidReport = nullptr; // start new report
				break;

			case HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM:
				hidGlobal->logicalMinimum = ItemSData(&item);
				break;

			case HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM:
				if (hidGlobal->logicalMinimum < 0)
					hidGlobal->logicalMaximum = ItemSData(&item);
				else
					hidGlobal->logicalMaximum = ItemUData(&item);
				break;

			case HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM:
				hidGlobal->physicalMinimum = ItemSData(&item);
				break;

			case HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM:
				if (hidGlobal->physicalMinimum < 0)
					hidGlobal->physicalMaximum = ItemSData(&item);
				else
					hidGlobal->physicalMaximum = ItemUData(&item);
				break;

			case HID_GLOBAL_ITEM_TAG_REPORT_SIZE:
				hidGlobal->reportSize = ItemUData(&item);
				break;

			case HID_GLOBAL_ITEM_TAG_REPORT_COUNT:
				hidGlobal->reportCount = ItemUData(&item);
				break;

			case HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
				hidGlobal->usagePage = ItemUData(&item);
				break;

			default:
				break;
			}

			break;

		case HID_TYPE_LOCAL:
			if (item.tag == HID_LOCAL_ITEM_TAG_USAGE)
			{
				hidLocal->usage = ItemUData(&item);

				if (g_HIDParseState.usagesCount < MAX_USAGE_NUM)
				{
					g_HIDParseState.usages[g_HIDParseState.usagesCount] = ItemUData(&item);
					g_HIDParseState.usagesCount++;
				}
			}
			else if (item.tag == HID_LOCAL_ITEM_TAG_USAGE_MIN)
				hidLocal->usageMin = ItemUData(&item);
			else if (item.tag == HID_LOCAL_ITEM_TAG_USAGE_MAX)
				hidLocal->usageMax = ItemUData(&item);

			break;
		}

		if (start == end)
		{
			 return true;
		}
	}

	return false;
}

uint32_t convert_range(const uint32_t value, const int16_t minimum, const uint16_t maximum, const preset_value_type target_type)
{
	preset_value_type source_type = VALUE_TYPE_CUSTOM;

	if ((minimum == 0) && (maximum == UINT8_MAX))
		source_type = VALUE_TYPE_UINT8;
	else if ((minimum == INT8_MIN) && (maximum == INT8_MAX))
		source_type = VALUE_TYPE_INT8;
	else if ((minimum == 0) && (maximum == UINT16_MAX))
		source_type = VALUE_TYPE_UINT16;
	if ((minimum == INT16_MIN) && (maximum == INT16_MAX))
		source_type = VALUE_TYPE_INT16;

	// uint16 -> uint16
	// int16 -> int16
	// uint8 -> uint8
	// int8 -> int8
	if (source_type == target_type)
		return value;

	if (source_type == VALUE_TYPE_INT8)
	{
		if (target_type == VALUE_TYPE_UINT8) // int8 -> uint8
			return ((int32_t)value) + 128;
		else if (target_type == VALUE_TYPE_UINT16) // int8 -> uint16
			return ((int32_t)value + 128) << 8;
		else if (target_type == VALUE_TYPE_INT16) // int8 -> int16
			return ((int32_t)value) << 8;
	}
	else if (source_type == VALUE_TYPE_UINT8)
	{
		if (target_type == VALUE_TYPE_INT8) // uint8 -> int8
			return ((int32_t)value) - 0x80;
		else if (target_type == VALUE_TYPE_UINT16) // uint8 -> uint16
			return value << 8;
		else if (target_type == VALUE_TYPE_INT16) // uint8 -> int16
			return ((int32_t)(value << 8)) - 0x8000;
	}
	else if (source_type == VALUE_TYPE_INT16)
	{
		if (target_type == VALUE_TYPE_UINT8) // int16 -> uint8
			return ((int32_t)value + 0x8000) >> 8;
		if (target_type == VALUE_TYPE_INT8) // int16 -> int8
			return ((int32_t)value) >> 8;
		else if (target_type == VALUE_TYPE_UINT16) // int16 -> uint16
			return ((int32_t)value) + 0x8000;
	}
	else if (source_type == VALUE_TYPE_UINT16)
	{
		if (target_type == VALUE_TYPE_UINT8)// uint16 -> uint8
			return value >> 8;
		else if (target_type == VALUE_TYPE_INT8) // uint16 -> int8
			return ((int32_t)(value >> 8)) - 0x80;
		else if (target_type == VALUE_TYPE_INT16)// uint16 -> int16
			return ((int32_t)value) - 0x8000;
	}

	// uint32_t and int32_t input types not implemented.
	// Custom input ranges not implemented.
	assert(false);

	return 0;
}

#define SetKey(key, report) (report->keyboard.keys[key >> 3] |= 1 << (key & 0x07))

typedef struct Mouse
{
	int16_t dx, dy, dz;
	uint8_t buttons;
	bool changed;
} Mouse;

static Mouse g_mouse;

void MouseSet(uint8_t button, bool state)
{
	if (state)
		g_mouse.buttons |= 1 << button;
	else
		g_mouse.buttons &= ~(1 << button);

	g_mouse.changed = true;
}

void MouseMove(int32_t dx, int32_t dy, int32_t dz)
{
	g_mouse.dx += dx;
	g_mouse.dy += dy;
	g_mouse.dz += dz;

	g_mouse.changed = true;
}

void processSeg(HID_SEG* segment, HID_REPORT* report, const uint8_t* data, gamepad_callback_t gamepad_callback)
{
	if (segment->inputType == MAP_TYPE_BITFIELD)
	{
		const uint16_t endbit = segment->startBit + segment->reportCount;
		uint8_t key_index = segment->outputControl;

		for (uint16_t cnt = segment->startBit; cnt < endbit; cnt++)
		{
			bool pressed = false;

			// find byte
			const uint8_t* currByte = data + ((cnt) >> 3);

			// find bit
			if (*currByte & (0x01 << (cnt & 0x07)))
				pressed = true;

			if (segment->outputChannel == MAP_KEYBOARD)
			{
				if (pressed)
					SetKey(key_index, report);
			}
			else if (segment->outputChannel == MAP_MOUSE)
			{
				switch (key_index)
				{
				case MAP_MOUSE_BUTTON1:
					MouseSet(0, pressed);
					break;
				case MAP_MOUSE_BUTTON2:
					MouseSet(1, pressed);
					break;
				case MAP_MOUSE_BUTTON3:
					MouseSet(2, pressed);
					break;
				case MAP_MOUSE_BUTTON4:
					MouseSet(3, pressed);
					break;
				case MAP_MOUSE_BUTTON5:
					MouseSet(4, pressed);
					break;
				}
			}

			key_index++;
		}
	}
	else if (segment->inputType) // i.e. not MAP_TYPE_NONE
	{
		uint32_t value = 0;

		// bits may be across any byte alignment
		// so do the neccesary shifting to get it to all fit in a uint32_t
		uint16_t currentBit = segment->startBit;

		for (uint8_t i = 0; i < segment->reportSize; i++)
		{
			// Calculate byte position and bit offset within the byte
			const uint8_t shiftbits = currentBit % 8;
			const uint8_t startbyte = currentBit / 8;

			// Extract the bit
			const uint8_t bit = (data[startbyte] >> shiftbits) & 0x01;

			// Add the bit to the result
			value |= (uint32_t)bit << i;

			currentBit++;
		}

		const bool sign = segment->logicalMinimum < 0;
		// if it's a signed integer we need to extend the sign
		if (sign)
			value = SIGNEX(value, segment->reportSize - 1);

		bool triggered = false;

		// Received HID value should be re-interpreted depending on Logical Minimum / Logical Maximum and Report Size in USB HID report descriptor.
		// Example:
		// Minimum 0xFF (-1), Maximum 0x01 (1), Report Size 0x08: value is signed int8_t in -1..1 range.
		// Minimum 0x81 (-127), Maximum 0x7F (127), Report Size 0x08: value is signed int8_t in -127..127 range.
		// Minimum 0x00 (0), Maximum 0xFF (255), Report Size 0x08: value is signed uint8_t in 0..255 range.

		// Note: Logical Minimum/Maximum can describe 16/32-bit ranges but require extended encoding
		// (0x81 for 16-bit, 0x82 for 32-bit), can be checked with Report Size.

		// Note: InputParam field in JoyPreset user presets is written assuming REPORT_USAGE_X / Y / Z / Rz
		// are unsigned 8-bit values in 0..255 range.
		// In practice they can be signed in -1..1 range for example, or 0..12000 / 0..65535 / -32768..32767 range with 16-bit ReportSize, or even 32-bit.

		// Map received value to 0..255 range depending on Logical Minimum / Logical Maximum.
		// ToDo: only map values with currSeg->InputUsage == REPORT_USAGE_X/Y
		if (segment->inputType == MAP_TYPE_THRESHOLD_ABOVE)
		{
			if (sign)
			{
				const uint8_t mapped_value = map_to_uint8((int32_t)value, segment->logicalMinimum, segment->logicalMaximum);

				triggered = (mapped_value > segment->inputParam);
			}
			else
			{
				const uint8_t mapped_value = map_to_uint8(value, segment->logicalMinimum, segment->logicalMaximum);

				triggered = (mapped_value > segment->inputParam);
			}
		}
		else if (segment->inputType == MAP_TYPE_THRESHOLD_BELOW)
		{

			if (sign)
			{
				const uint8_t mapped_value = map_to_uint8((int32_t)value, segment->logicalMinimum, segment->logicalMaximum);

				triggered = (mapped_value < segment->inputParam);
			}
			else
			{
				const uint8_t mapped_value = map_to_uint8(value, segment->logicalMinimum, segment->logicalMaximum);

				triggered = (mapped_value < segment->inputParam);
			}
		}
		else if (segment->inputType == MAP_TYPE_EQUAL)
		{
			triggered = (value == segment->inputParam);
		}

		if (triggered)
		{
			if (segment->outputChannel == MAP_KEYBOARD)
			{
				SetKey(segment->outputControl, report);
			}

			if (segment->outputChannel == MAP_GAMEPAD)
			{
				if(gamepad_callback)
					gamepad_callback(segment->outputControl, 1);
			}
		}
		else if (segment->inputType == MAP_TYPE_AXIS)
		{
			if (segment->outputChannel == MAP_GAMEPAD)
			{
				const uint8_t axis_value = convert_range(value, segment->logicalMinimum, segment->logicalMaximum, (preset_value_type)segment->inputParam);

				if(gamepad_callback)
					gamepad_callback(segment->outputControl, axis_value);
			}
		}
		else if (segment->inputType == MAP_TYPE_SCALE)
		{
			if (segment->outputChannel == MAP_MOUSE)
			{
				switch (segment->outputControl)
				{
				case MAP_MOUSE_X:
					MouseMove((int32_t)value, 0, 0);
					break;
				case MAP_MOUSE_Y:
					MouseMove(0, (int32_t)value, 0);
					break;
				case MAP_MOUSE_WHEEL:
					MouseMove(0, 0, (int32_t)value);
					break;
				}
			}
		}
		else if (segment->inputType == MAP_TYPE_ARRAY)
		{
			if (segment->outputChannel == MAP_KEYBOARD)
			{
				if (value)
					SetKey((uint8_t)value, report);
			}
		}
	}
}

HID_REPORT* find_report_parser(HID_REPORT* head, uint8_t reportID)
{
	while (head)
	{
		if (head->reportID == reportID)
			return head;

		head = head->next;
	}

	return nullptr;
}

bool ParseReport(const uint8_t* report, const uint32_t len,
	gamepad_callback_t gamepad_callback, keyboard_callback_t keyboard_callback, mouse_callback_t mouse_callback)
{
	HID_REPORT* reportDesc = nullptr;

	if (g_interface_uses_reports)
	{
		// first byte of report will be the report number
		reportDesc = find_report_parser(g_reports, report[0]);
	}
	else
	{
		reportDesc = g_reports;
	}

	if (reportDesc == nullptr)
	{
		printf("Invalid report\n");
		return false;
	}

	if (len < (reportDesc->length >> 3))
	{
		printf("Report too short - %lu bytes < %u bits\n", len, reportDesc->length);
		return false;
	}

	HID_SEG* segment = reportDesc->segments;

	while (segment)
	{
		processSeg(segment, reportDesc, report, gamepad_callback);
		segment = segment->next;
	}

	if (keyboard_callback)
	{
		keyboard_state* keyboard = &reportDesc->keyboard;

		for (uint8_t byte = 0; byte < KEYBOARD_STATE_SIZE; byte++)
		{
			// XOR to see if any bits are different
			const uint8_t xorred = keyboard->keys[byte] ^ keyboard->old_keys[byte];

			if (xorred)
			{
				for (uint8_t bit = 0; bit < 8; bit++)
				{
					if (xorred & (1 << bit))
					{
						const uint8_t hid_code = (byte << 3) | bit;
						const bool pressed = keyboard->keys[byte] & (1 << bit);

						keyboard_callback(hid_code, pressed);
					}
				}
			}
		}

		memcpy(keyboard->old_keys, keyboard->keys, KEYBOARD_STATE_SIZE);
		memset(keyboard->keys, 0, KEYBOARD_STATE_SIZE);
	}

	if (mouse_callback)
	{
		if (g_mouse.changed)
		{
			mouse_callback(g_mouse.dx, g_mouse.dy, g_mouse.dz, g_mouse.buttons);

			g_mouse = {};
		}
	}

	return true;
}
