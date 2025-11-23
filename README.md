#Sega Mega Drive to GameCube controller adapter

## Schematics

![Schematics](images/scheme.png)

## Bill of Materials
- GameCube controller replacement cable
- WaveShare RP2040-Zero board
- DB9 Male connector
- 12 wires (approx. 60 mm each)

# Buttons mapping
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
gc.dRight = smd.right;
gc.dDown = smd.down;
gc.dUp = smd.up;

gcReport.z = smd.mode;
```

## How to flash firmware
- Connect RP2040-Zero to PC
- Press BOOT and then RESET, new drive will appear
- Drop SMD2GC.uf2 from Releases section to new drive

## How to build firmware
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