#include <array>

#include "communication_protocols/joybus/gcReport.hpp"

/*
	GPIO pins connection to Sega Mega Drive Controller DB9 Male connector:
	Signal   D0 D1 D2 D3 D4 D5 SEL GND VCC
	DB9 pin  1  2  3  4  6  9  7   8   5
	GPIO pin 0  1  2  3  4  5  7   GND 3V3
*/
constexpr uint SMD_DATA_PIN0 = 0;
constexpr uint SMD_DATA_PIN1 = 1;
constexpr uint SMD_DATA_PIN2 = 2;
constexpr uint SMD_DATA_PIN3 = 3;
constexpr uint SMD_DATA_PIN4 = 4;
constexpr uint SMD_DATA_PIN5 = 5;
constexpr uint SMD_SELECT_PIN = 7;

struct __attribute__((packed)) smd_state
{
	uint16_t connected : 1,
	six_buttons : 1,
	a : 1,
	b : 1,
	c : 1,
	x : 1,
	y : 1,
	z : 1,
	start : 1,
	mode :1,
	up : 1,
	down : 1,
	left : 1,
	right : 1;
};

struct smd_state smd;

void initSegaMegaDrive()
{
	std::array<uint8_t,6> smd_gpio = 
	{
		SMD_DATA_PIN0,
		SMD_DATA_PIN1,
		SMD_DATA_PIN2,
		SMD_DATA_PIN3,
		SMD_DATA_PIN4,
		SMD_DATA_PIN5
	}; // SMD data pins

	for (uint8_t pin : smd_gpio)
	{
		gpio_init(pin);
		gpio_set_dir(pin, GPIO_IN);
		gpio_pull_up(pin); // For correct controller presence detection.
	}

	// SMD SEl pin
	gpio_init(SMD_SELECT_PIN);
	gpio_set_dir(SMD_SELECT_PIN, GPIO_OUT);
	gpio_disable_pulls(SMD_SELECT_PIN);
}

/*
	References:
	https://www.raspberryfield.life/2019/03/25/sega-mega-drive-genesis-6-button-xyz-controller/
	https://segaretro.org/Six_Button_Control_Pad_(Mega_Drive)
	https://segaretro.org/File:Genesis_Software_Manual.pdf
	Questionable:
	https://segaretro.org/Sega_Mega_Drive/Control_pad_inputs
*/
GCReport getSegaMegaDriveReport()
{
	GCReport gcReport = defaultGcReport;

	static uint32_t last_update_time = 0;
	uint32_t current_time = time_us_32();

	constexpr uint32_t SMD_RESET_DELAY = 3*1000; // Must be >= 3 to give 6-button controller time to reset

	// check last read time; return last result if < 3 ms passed (required for 6 buttons pads to reset state)
	if((current_time - last_update_time) >= SMD_RESET_DELAY)
	{
		last_update_time = current_time;
/*
    Cycle  SEL out D5 in  D4 in  D3 in  D2 in  D1 in  D0 in
    0      HI      C      B      Right  Left   Down   Up      (Read B, C and directions in this cycle)
    1      LO      Start  A      0      0      Down   Up      (Check connected and read Start and A in this cycle)
    2      HI      C      B      Right  Left   Down   Up
    3      LO      Start  A      0      0      Down   Up
    4      HI      C      B      Right  Left   Down   Up
    5      LO      Start  A      0      0      0      0       (Check for six button controller in this cycle (D0 and D1 == LOW))
    6      HI      C      B      Mode   X      Y      Z       (Read X,Y,Z and Mode in this cycle)
    7      LO      ---    ---    ---    ---    ---    ---     (Ignored)
*/
		size_t constexpr CYCLES_COUNT = 8; // 8 iterations for read controller state.

		uint8_t smd_data[CYCLES_COUNT] = {};

		bool select = true;
		gpio_put(SMD_SELECT_PIN, select);

		for (size_t i = 0; i < CYCLES_COUNT; i++)
		{
/*
	Genesis Software Manual (C) 1989 Sega of Japan:
	Genesis Technical Bulletin #27 January 24, 1994:
	1. Warning regarding control pad data reads
	For both the 3 and 6 button pads, the pad data is determined 2usec after TH is modified.
	Therefore, as shown in the sample below, read data from the pad 2usec (4 nop's) after TH is modified.
	Joypad Reads
	Pad data from the 3 and 6 button controllers are read 2usec after TH is modified.
	The wait is necessary because the data in the chip needs time to stabilize after TH is modified.
	If data is read without this wait, there is no guarantee that the data will be correct.
	Moreover, the 2usec time is equivalent to 4 nop, including the 68000's prefetch.
*/
			sleep_us(2); // Short delay to stabilise outputs in controller.

			smd_data[i] = gpio_get(SMD_DATA_PIN0)
						| gpio_get(SMD_DATA_PIN1) << 1
						| gpio_get(SMD_DATA_PIN2) << 2
						| gpio_get(SMD_DATA_PIN3) << 3
						| gpio_get(SMD_DATA_PIN4) << 4
						| gpio_get(SMD_DATA_PIN5) << 5;

			select = !select;
			gpio_put(SMD_SELECT_PIN, select);
		}

		smd.connected = !(smd_data[1] & 0b001100);

		if (smd.connected)
		{
			// Fixed: 6-buttons controller detection:
			smd.six_buttons =  ((smd_data[5] & 0b011) == 0);

			smd.a = !!(~smd_data[1] & 0b010000);
			smd.b = !!(~smd_data[0] & 0b010000);
			smd.c = !!(~smd_data[0] & 0b100000);
			smd.x = !!(smd.six_buttons && (~smd_data[6] & 0b000100));
			smd.y = !!(smd.six_buttons && (~smd_data[6] & 0b000010));
			smd.z = !!(smd.six_buttons && (~smd_data[6] & 0b000001));
			smd.up = !!(~smd_data[0] & 0b000001);
			smd.down = !!(~smd_data[0] & 0b000010);
			smd.left = !!(~smd_data[0] & 0b000100);
			smd.right = !!(~smd_data[0] & 0b001000);
			smd.start = !!(~smd_data[1] & 0b100000);
			smd.mode = !!(smd.six_buttons && (~smd_data[6] & 0b001000));
		}
		else
		{
			smd = {};
		}
	}

	gcReport.a = smd.a;
	gcReport.b = smd.b;
	gcReport.x = smd.x;
	gcReport.y = smd.y;
	gcReport.l = smd.z;
	gcReport.r = smd.c;

	gcReport.start = smd.start;

	gcReport.dLeft = smd.left;
	gcReport.dRight = smd.right;
	gcReport.dDown = smd.down;
	gcReport.dUp = smd.up;
	gcReport.z = smd.mode;

	return gcReport;
}