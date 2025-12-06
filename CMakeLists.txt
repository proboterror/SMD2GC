# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD waveshare_rp2040_zero CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 14) # or 11, 17, 20, 23
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(PICO_SDK_FETCH_FROM_GIT 1)

include(pico_sdk_import.cmake)

pico_sdk_init()

project(SMD2GC CXX C)

include_directories(include pico-joybus-comms/include)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/tusb_xinput xinput_host)

add_executable(
  ${PROJECT_NAME}
  src/main.cpp
  src/arena_allocator.cpp
  src/hid_gamecube_mapping.cpp
  src/hid_parser.cpp
  src/hid_tests.cpp
  src/ps3.cpp
  src/sega_mega_drive.cpp
  src/communication_protocols/joybus.cpp
)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/pio/my_pio.pio)
add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/my_pio.pio.h
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/my_pio.pio
        COMMAND Pioasm ${CMAKE_CURRENT_LIST_DIR}/my_pio.pio ${CMAKE_CURRENT_LIST_DIR}/generated/my_pio.pio.h
        )
target_link_libraries(${PROJECT_NAME} pico_stdlib tinyusb_host tinyusb_board tinyusb_common xinput_host pico_multicore pico_sync hardware_pio pico_time hardware_resets hardware_timer hardware_irq hardware_sync)
pico_add_extra_outputs(${PROJECT_NAME})

# Expose TinyUSB headers for includes like "host/usbh.h"
set(TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${TINYUSB_PATH}/src
    ${TINYUSB_PATH}/hw  # For board.h if needed
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Define macros for UART1 stdio
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PICO_DEFAULT_UART=1
    PICO_DEFAULT_UART_TX_PIN=8
    PICO_DEFAULT_UART_RX_PIN=9
    PICO_DEFAULT_UART_BAUD_RATE=115200

    # This is the critical flag to ensure the native HCD source files are compiled
    # It activates the host part of the TinyUSB wrapper in the Pico SDK.
    PICO_TINYUSB_HOST=1

    TUSB_CONFIG_FILE="tusb_config.h"

    CFG_TUH_ENABLED=1
    CFG_TUSB_MCU=OPT_MCU_RP2040
    # Defines the hardware USB port as Root Hub Port 1
    BOARD_TUH_RHPORT=0
 )

# For Waveshare RP2040-Zero you must tell the SDK which pins are USB
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PICO_USB_HOST_DP_PIN=19
    PICO_USB_HOST_DM_PIN=18
)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)