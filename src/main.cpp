#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "pico/stdio_uart.h"
#include "hardware/clocks.h"

#include "host/usbh.h"
#include "tusb.h"

#include "sega_mega_drive.h"
#include "communication_protocols/joybus.hpp"

// Minimal frequency required for stable USB Host support on RP2040 is 144 MHz (multiple of 48 MHz USB clock).
// GameCube JoyBus support pio program timings are for 25 MHz clock.
// JoyBus pio program clock divider set to 6 (150/6).
// 125 MHz with pio divider set to 5 working with DualShock 4, Xbox Series Model 1914 Controller; Xbox 360 controller unstable, DualSense attachment not detected (VBUS below 5V issue).
const int FREQUENCY_MHZ = 150;

GCReport globalGCState = defaultGcReport;
spin_lock_t* shared_data_lock = nullptr;
bool usb_gamepad_connected = false;

GCReport getControllerState()
{
	if(usb_gamepad_connected)
	{
		//spin_lock_unsafe_blocking(shared_data_lock);
		GCReport report = globalGCState;
		//spin_unlock_unsafe(shared_data_lock);
		return report;
	}
	else
	{
		return getSegaMegaDriveReport();
	}
}

#include "xinput_host.h"

//Since https://github.com/hathach/tinyusb/pull/2222, we can add in custom vendor drivers easily
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count)
{
	*driver_count = 1;

	return &usbh_xinput_driver;
}

static inline uint8_t int16_to_u8_biased(int16_t x)
{
	return (uint8_t)((uint32_t)(x + 32768) >> 8);
}

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, xinputh_interface_t const* xid_itf, uint16_t len)
{
	const xinput_gamepad_t *pad = &xid_itf->pad;
	const char* type_str;

	if (xid_itf->last_xfer_result == XFER_RESULT_SUCCESS)
	{
		switch (xid_itf->type)
		{
			case 1: type_str = "Xbox One"; break;
			case 2: type_str = "Xbox 360 Wireless"; break;
			case 3: type_str = "Xbox 360 Wired"; break;
			case 4: type_str = "Xbox OG"; break;
			default: type_str = "Unknown";
		}

		if (xid_itf->connected && xid_itf->new_pad_data)
		{
			TU_LOG2("[%02x, %02x], Type: %s, Buttons %04x, LT: %02x RT: %02x, LX: %d, LY: %d, RX: %d, RY: %d\n",
				dev_addr, instance, type_str, pad->wButtons, pad->bLeftTrigger, pad->bRightTrigger, pad->sThumbLX, pad->sThumbLY, pad->sThumbRX, pad->sThumbRY);

			GCReport gc = defaultGcReport;
			const uint16_t buttons = pad->wButtons;

			gc.a = (buttons & XINPUT_GAMEPAD_A) != 0;
			gc.b = (buttons & XINPUT_GAMEPAD_B) != 0;
			gc.x = (buttons & XINPUT_GAMEPAD_X) != 0;
			gc.y = (buttons & XINPUT_GAMEPAD_Y) != 0;
			gc.start = (buttons & (XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_GUIDE)) != 0;

			gc.dLeft = (buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
			gc.dRight = (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
			gc.dDown = (buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
			gc.dUp = (buttons & XINPUT_GAMEPAD_DPAD_UP) != 0;

			static const uint8_t TRIGGER_CLICK_TRESHOLD = 32;

			gc.l = pad->bLeftTrigger > TRIGGER_CLICK_TRESHOLD;
			gc.r = pad->bRightTrigger > TRIGGER_CLICK_TRESHOLD;
			//gc.l = (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
			//gc.r = (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
			gc.z = (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;

			gc.xStick = int16_to_u8_biased(pad->sThumbLX);
			gc.yStick = int16_to_u8_biased(pad->sThumbLY);
			gc.cxStick = int16_to_u8_biased(pad->sThumbRX);
			gc.cyStick = int16_to_u8_biased(pad->sThumbRY);
			gc.analogL = pad->bLeftTrigger;
			gc.analogR = pad->bRightTrigger;

			//spin_lock_unsafe_blocking(shared_data_lock);
			globalGCState = gc;
			//spin_unlock_unsafe(shared_data_lock);
		}
	}

	tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t *xinput_itf)
{
	TU_LOG1("XInput Mounted %02x %d\n", dev_addr, instance);
	// If this is a Xbox 360 Wireless controller we need to wait for a connection packet
	// on the in pipe before setting LEDs etc. So just start getting data until a controller is connected.
	if (xinput_itf->type == XBOX360_WIRELESS && xinput_itf->connected == false)
	{
		tuh_xinput_receive_report(dev_addr, instance);

		return;
	}

	tuh_xinput_set_led(dev_addr, instance, 0, true);
	tuh_xinput_set_led(dev_addr, instance, 1, true);
	tuh_xinput_set_rumble(dev_addr, instance, 0, 0, true);
	tuh_xinput_receive_report(dev_addr, instance);

	usb_gamepad_connected = true;
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance)
{
	TU_LOG1("XInput Unmounted %02x %d\n", dev_addr, instance);

	usb_gamepad_connected = false;
}

void core1_main(void)
{
	if (!tuh_init(BOARD_TUH_RHPORT))
	{
		printf("Failed to initialize TinyUSB Host\n");
		return;
	}

	while(true)
	{
		tuh_task();
		tight_loop_contents(); // sleep_us(100);
	}
}

int main()
{
	set_sys_clock_khz(1000 * FREQUENCY_MHZ, true);

	stdio_uart_init();
	stdio_init_all();

	printf("SMD2GC Sega Mega Drive / USB HID to GameCube adapter\nhttps://github.com/proboterror/SMD2GC\n");

	uint lock_num = spin_lock_claim_unused(true); // Low-level: returns uint ID; panic if none free
	if (lock_num == (uint)-1) // No free locks (rare, but check)
	{
		panic("No free spinlocks available!");
	}
	shared_data_lock = spin_lock_init(lock_num); // High-level: convert uint to spin_lock_t*

	multicore_launch_core1(core1_main);

	initSegaMegaDrive();

	CommunicationProtocols::Joybus::enterMode(
			[]() {
				return getControllerState();
			});
}
