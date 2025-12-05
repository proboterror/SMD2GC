# Sega Mega Drive to GameCube controller adapter

Also supports Xbox controllers (Xbox One / Series) and standard USB HID gamepads (with layout similar to PS4 DualShock 4) with USB Type-C OTG adapter.

USB gamepads support can be limited because of GameCube controller port does not provide sufficient +5V power (Xbox 360 USB pad unstable, PS5 DualSense not working).

Bluetooth dongles are not supported unless they emulate standard USB HID device.

DualShock 4 / Xbox One / Series controllers are recommended.

## Schematics

![Schematics](images/scheme.png)

## Bill of Materials
- GameCube controller replacement cable
- WaveShare RP2040-Zero board
- DB9 Male connector
- 12 wires (approx. 60 mm each)

# Buttons mapping
## Sega Mega Drive 6-button pad
src/sega_mega_drive.cpp:
```
gc.a = smd.a;
gc.b = smd.b;
gc.x = smd.x;
gc.y = smd.y;

gc.l = smd.z;
gc.r = smd.c;

gc.start = smd.start;

gc.dLeft = smd.left;
gc.dRight = smd.right
gc.dDown = smd.down
gc.dUp = smd.up
gcReport.z = smd.mode
```
## Xbox controllers
src/main.c:
```
gc.a = (buttons & XINPUT_GAMEPAD_A);
gc.b = (buttons & XINPUT_GAMEPAD_B);
gc.x = (buttons & XINPUT_GAMEPAD_X);
gc.y = (buttons & XINPUT_GAMEPAD_Y);

gc.start = (buttons & (XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_GUIDE));

gc.dLeft = (buttons & XINPUT_GAMEPAD_DPAD_LEFT);
gc.dRight = (buttons & XINPUT_GAMEPAD_DPAD_RIGHT);
gc.dDown = (buttons & XINPUT_GAMEPAD_DPAD_DOWN);
gc.dUp = (buttons & XINPUT_GAMEPAD_DPAD_UP);

static const uint8_t TRIGGER_CLICK_TRESHOLD = 32;
gc.l = pad->bLeftTrigger > TRIGGER_CLICK_TRESHOLD
gc.r = pad->bRightTrigger > TRIGGER_CLICK_TRESHOLD;

gc.z = (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

gc.xStick = int16_to_u8_biased(pad->sThumbLX);
gc.yStick = int16_to_u8_biased(pad->sThumbLY);

gc.cxStick = int16_to_u8_biased(pad->sThumbRX);
gc.cyStick = int16_to_u8_biased(pad->sThumbRY);

gc.analogL = pad->bLeftTrigger;
gc.analogR = pad->bRightTrigger;
```

## Standard USB HID controllers
src/hid_gamecube_mapping.cpp:
```
REPORT_USAGE_PAGE_BUTTON, 1, MAP_GAMECUBE_BUTTON_Y // Square -> Y
REPORT_USAGE_PAGE_BUTTON, 2, MAP_GAMECUBE_BUTTON_A // Cross -> A
REPORT_USAGE_PAGE_BUTTON, 3, MAP_GAMECUBE_BUTTON_B // Circle -> B
REPORT_USAGE_PAGE_BUTTON, 4, MAP_GAMECUBE_BUTTON_X // Triangle -> X

REPORT_USAGE_PAGE_BUTTON, 6, MAP_GAMECUBE_BUTTON_Z // R1

// Note: DualShock S4 generates press event starting from minimum force,
// GameCube controller generates L/R clicks after maximum force to analog L/R axes applied.
// Can be replaced with mapping analog Rx/Ry axis with MAP_TYPE_THRESHOLD_ABOVE 192 (75%) to GameCube L/R.
REPORT_USAGE_PAGE_BUTTON, 7, MAP_GAMECUBE_BUTTON_L // L2 Digital
REPORT_USAGE_PAGE_BUTTON, 8, MAP_GAMECUBE_BUTTON_R // R2 Digital

REPORT_USAGE_PAGE_BUTTON, 10, MAP_GAMECUBE_BUTTON_START // Options -> Start

// Left analog
REPORT_USAGE_X, MAP_GAMECUBE_AXIS_X // LX
REPORT_USAGE_Y, MAP_GAMECUBE_AXIS_Y // LY
// Right analog
REPORT_USAGE_Z, MAP_GAMECUBE_AXIS_CX // RX
REPORT_USAGE_Rz, MAP_GAMECUBE_AXIS_CY // RY
// Analog left/right triggers
REPORT_USAGE_Rx, MAP_GAMECUBE_AXIS_L // LT / R2
REPORT_USAGE_Ry, MAP_GAMECUBE_AXIS_R // RT / R2

// POV/HAT switch (also D-PAD in most cases)
// 0 on hat switch, just press up
MAP_GAMECUBE_U, HID_GAMEPAD_HAT_UP
// 1 on hat switch, press up and right
HID_GAMEPAD_HAT_UP_RIGHT, MAP_GAMECUBE_U
HID_GAMEPAD_HAT_UP_RIGHT, MAP_GAMECUBE_R
// 2 on hat switch, press right
HID_GAMEPAD_HAT_RIGHT, MAP_GAMECUBE_R
// 3 on hat, press right and down
HID_GAMEPAD_HAT_DOWN_RIGHT, MAP_GAMECUBE_R
HID_GAMEPAD_HAT_DOWN_RIGHT, MAP_GAMECUBE_D
// 4 on hat, press down
HID_GAMEPAD_HAT_DOWN, MAP_GAMECUBE_D
// 5 on hat, press down and left
HID_GAMEPAD_HAT_DOWN_LEFT, MAP_GAMECUBE_D
HID_GAMEPAD_HAT_DOWN_LEFT, MAP_GAMECUBE_L
// 6 on hat, press left
HID_GAMEPAD_HAT_LEFT, MAP_GAMECUBE_L
// 7 on hat, press left and up
HID_GAMEPAD_HAT_UP_LEFT, MAP_GAMECUBE_L
HID_GAMEPAD_HAT_UP_LEFT, MAP_GAMECUBE_U

// Rarely used
REPORT_USAGE_DPAD_UP, MAP_GAMECUBE_U
REPORT_USAGE_DPAD_DOWN, MAP_GAMECUBE_D
REPORT_USAGE_DPAD_RIGHT, MAP_GAMECUBE_R
REPORT_USAGE_DPAD_LEFT, MAP_GAMECUBE_L

PS4 DualShock 4 / PS5 DualSense used as reference for buttons IDs:
Button 0x01 - Square button
Button 0x02 - Cross button
Button 0x03 - Circle button
Button 0x04 - Triangle button
Button 0x05 - L1 button
Button 0x06 - R1 button
Button 0x07 - L2 button
Button 0x08 - R2 button
Button 0x09 - Share button
Button 0x0A - Options button
Button 0x0B - L3 button
Button 0x0C - R3 button
Button 0x0D - PS button
Button 0x0E - Touchpad button
```

#Input lag
For Sega Mega Drive 6 buttons pad delay mostly defined by GameCube controller JoyBus speed if queried once per frame:<br>
3 bytes console request + 8 bytes response @250kbps: protocol delay ~0.35 ms.

USB pads are reports its state at fixed rate, mostly 125Hz (8 ms latency).<br>
DualShock 4 has a default wired polling rate 250Hz (4ms latency)<br>
Xbox Series Controller Model 1914: default USB poll rate 125Hz (8 ms latency)

## How to flash firmware
- Connect RP2040-Zero to PC
- Press BOOT and then RESET, new drive will appear
- Drop SMD2GC.uf2 from Releases section to new drive

## How to build firmware
Checkhout git repository.
```
git submodule update --init
```
Easy way:
- Install Visual Studio Code and official Raspberry Pi Pico Visual Studio Code extension
- On "Raspberry Pi Pico Project" left tab: "Import project"
- "Configure Cmake", use toolchain installed with Raspberry Pi Pico Visual Studio Code extension
- "Compile Project"
- "Run Project (USB)". Board should be in BOOT mode.

Hard way:
- Install Raspberry Pi Pico SDK, ARM-GCC toolchain, CMake.

## Credits
- [Julien Bernard](https://github.com/JulienBernard3383279/pico-rectangle) - Joybus protocol (Gamecube controller) implementation for the Raspberry Pi Pico
- [riguetti](https://github.com/riguetti/RP2040-Zero-gamecube-controller) - unused code cleanup
- [Andrew Tait](https://github.com/rasteri) - USB HID descriptors and reports parsing code
- [Ryzee119](https://github.com/Ryzee119/tusb_xinput) - Xbox/Xinpit controllers support module for TinyUSB library