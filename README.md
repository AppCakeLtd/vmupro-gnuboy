# VMUPro Gnuboy Game Boy Emulator

A Game Boy and Game Boy Color emulator for the [VMUPro](https://8bitmods.com/vmupro-handheld-visual-memory-card-for-dreamcast-smoke-black/) handheld device, based on the Gnuboy emulator core.

## Overview

This project ports the popular Gnuboy Game Boy emulator to run on the VMUPro platform, allowing you to play classic Game Boy and Game Boy Color games on your VMUPro device. The emulator includes support for multiple Memory Bank Controllers (MBCs) and provides smooth gameplay with accurate emulation.

## Features

- Full Game Boy and Game Boy Color emulation using the Gnuboy core
- Support for multiple Memory Bank Controllers (MBC1, MBC2, MBC3, MBC5)
- Audio emulation with all four Game Boy sound channels
- Picture Processing Unit (PPU) emulation for accurate graphics
- Battery-backed save RAM (SRAM) support with persistence
- Built-in ROM browser and file manager
- Pause menu with save/load functionality
- Multiple Game Boy color palettes (37 total including DMG, MGB, CGB, SGB)
- Configurable brightness and volume controls
- Frame rate monitoring and performance optimization
- Controller input support via VMUPro buttons

## Supported Memory Bank Controllers

The emulator supports the most common Game Boy cartridge types:
- **MBC0** (NROM) - No memory banking
- **MBC1** - ROM/RAM banking up to 2MB ROM / 32KB RAM
- **MBC2** - ROM banking with built-in 512x4bit RAM
- **MBC3** - ROM/RAM banking with Real-Time Clock (RTC)
- **MBC5** - Advanced banking for larger ROMs up to 8MB

## Requirements

- VMUPro device
- VMUPro SDK (included as submodule)
- ESP-IDF development framework
- Game Boy ROM files (.gb format) and Game Boy Color ROM files (.gbc format)

## Building

### Prerequisites

1. Install ESP-IDF and set up your development environment:
   ```bash
   . /opt/idfvmusdk/export.sh
   ```

2. Clone this repository with submodules:
   ```bash
   git clone --recursive https://github.com/8BitMods/vmupro-gnuboy.git
   cd vmupro-gnuboy/src
   ```

### Build Process

1. Build the project:
   ```bash
   idf.py build
   ```

2. Package the application:
   ```bash
   ./pack.sh
   ```
   Or on Windows:
   ```powershell
   ./pack.ps1
   ```

3. Deploy to VMUPro:
   ```bash
   ./send.sh <comport>
   ```
   Or on Windows:
   ```powershell
   ./send.ps1 <comport>
   ```

## Usage

1. Create a `/sdcard/roms/GameBoy/` directory on your VMUPro device
2. Place your Game Boy ROM files (.gb) and Game Boy Color ROM files (.gbc) in this directory
3. Launch the Gnuboy emulator from the VMUPro menu
4. Select a ROM file from the built-in browser
5. Use VMUPro controls to play:
   - **D-pad**: Game Boy D-pad
   - **Btn_A**: Game Boy A button
   - **Btn_B**: Game Boy B button
   - **Btn_Power**: Game Boy SELECT button
   - **Btn_Mode**: Game Boy START button
   - **Btn_Bottom**: Pause menu toggle

### Pause Menu Options

Press **Btn_Bottom** during gameplay to access:
- **Save State**: Save current game progress
- **Load State**: Restore saved game progress
- **Brightness**: Adjust screen brightness (5 levels)
- **Volume**: Adjust audio volume (5 levels)
- **Palette**: Choose from 37 different color palettes
- **Quit**: Return to ROM browser

## Project Structure

```
src/
├── main/
│   ├── main.cpp              # Main emulator entry point and menu system
│   └── gnuboy/               # Gnuboy emulator core
│       ├── cpu.c             # Sharp LR35902 CPU emulation
│       ├── lcd.c             # LCD controller and PPU
│       ├── sound.c           # Audio Processing Unit (APU)
│       ├── mem.c             # Memory management and banking
│       ├── rtc.c             # Real-Time Clock (MBC3)
│       └── ...               # Additional core components
├── metadata.json             # VMUPro app metadata
├── icon.bmp                  # Application icon
├── pack.sh/pack.ps1          # Packaging scripts
└── send.sh/send.ps1          # Deployment scripts
```

## Technical Details

- **Platform**: ESP32-based VMUPro
- **Framework**: ESP-IDF with VMUPro SDK
- **Emulator Core**: Gnuboy
- **Display**: 240x240 VMUPro LCD scaling 160x144 Game Boy screen
- **Audio**: 44.1kHz stereo output via VMUPro audio system
- **Input**: VMUPro button mapping with pause menu overlay
- **Memory**: Double-buffered rendering with optimized frame timing
- **Storage**: SRAM saves stored as .sram files alongside ROMs

## Performance

The emulator is optimized for VMUPro hardware:
- **Target**: 60 FPS with frame skip when needed
- **Audio**: Real-time streaming with 736-sample buffer
- **Memory**: Efficient ROM banking and video buffer management
- **Timing**: Microsecond-level frame pacing for smooth gameplay

## File Organization

Game Boy and Game Boy Color ROMs should be placed in `/sdcard/roms/GameBoy/` on your VMUPro device. Save files are automatically created as `.sram` files in the same directory when games support battery-backed saves.

Example structure:
```
/sdcard/roms/GameBoy/
├── Tetris.gb
├── Pokemon Red.gb
├── Pokemon Red.sram          # Auto-generated save file
├── Pokemon Gold.gbc          # Game Boy Color ROM
├── Pokemon Gold.sram         # Auto-generated save file
├── Super Mario Land.gb
└── Zelda - Links Awakening.gb
```

## Credits

- **Gnuboy Emulator**: Original Game Boy emulator core by Laguna
- **8BitMods**: VMUPro porting and optimization
- **VMUPro SDK**: Platform-specific libraries and tools

## License

This project is licensed under the GNU General Public License v2.0. See the [LICENSE](LICENSE) file for details.

The Gnuboy emulator core maintains its original licensing terms.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Support

For support and questions:
- Check the VMUPro documentation
- Report issues on the project repository
- Join the 8BitMods community

---

Enjoy playing classic Game Boy and Game Boy Color games on your VMUPro device!