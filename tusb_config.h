#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------
// Set the controller mode to Host for the RP2040 USB port (Port 0)
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_HOST 
#define CFG_TUSB_OS             OPT_OS_PICO

// Debug level (0-3). 0 = no debug output; Level 2: Full control transfer logs (descriptor gets, errors)
//#define CFG_TUSB_DEBUG          0
// Host-specific: Pipe opens, enumeration steps
//#define CFG_TUH_DEBUG           0

//--------------------------------------------------------------------
// HOST CONFIGURATION
//--------------------------------------------------------------------
#define CFG_TUH_DEVICE_MAX          2 // At least 2, 4 is safe
#define CFG_TUH_HUB                 1 // Support hubs (recommended)

// Size of the enumeration buffer (typically 256 bytes is enough)
#define CFG_TUH_ENUMERATION_BUFSIZE 512

//--------------------------------------------------------------------
// CLASS DRIVERS
//--------------------------------------------------------------------
#define CFG_TUH_HID 1
#define CFG_TUH_XINPUT 1

// HID report descriptor buffer
#define CFG_TUH_HID_DESC_BUFSIZE  1024

#endif