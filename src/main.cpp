#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#include "sega_mega_drive.h"
#include "communication_protocols/joybus.hpp"

const int FREQUENCY_MHZ = 125;

int main()
{
	set_sys_clock_khz(1000 * FREQUENCY_MHZ, true);
	stdio_init_all();

	initSegaMegaDrive();

	CommunicationProtocols::Joybus::enterMode(
			[]() {
				return getSegaMegaDriveReport();
			});
}
