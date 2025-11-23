#ifndef COMMUNICATION_PROTOCOLS__JOYBUS_HPP
#define COMMUNICATION_PROTOCOLS__JOYBUS_HPP

#include "pico/stdlib.h"
#include "joybus/gcReport.hpp"

#include <functional>

namespace CommunicationProtocols {
namespace Joybus {

const uint dataPin = 15;
const uint rumblePin = 16;

/**
 * @short Enters the Joybus communication mode
 * 
 * @param dataPin GPIO number of the console data line pin
 * @param func Function to be called to obtain the GCReport to be sent to the console
 */
void enterMode(std::function<GCReport()> func);

}
}

#endif
