# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
This is a Game Boy emulator (Gnuboy) ported to the VMUPro platform. The project integrates the Gnuboy emulator with the VMUPro SDK to create a Game Boy emulator that runs on VMU hardware. The emulator includes a full game selection interface, save/load functionality, and configurable settings for brightness, volume, and display palettes.

## Build System & Commands

### Essential Build Commands
- **Build:** `idf.py build` (from src/ directory)
- **Package:** `./pack.sh` (creates gnuboy.vmupack file)  
- **Deploy:** `./send.sh <comport>` (uploads and optionally runs on device)
- **Deploy (Windows):** `./send.ps1 <comport>`

### Build Prerequisites
- ESP-IDF environment must be set up: `. /opt/idfvmusdk/export.sh` (Linux/macOS)
- VMUPro SDK dependency (submodule at vmupro-sdk/)

### Build Configuration
- CMake-based build system using ESP-IDF framework
- Main CMakeLists.txt in src/ directory configures the project as "gnuboy"
- ELF loader support enabled for dynamic loading
- Optimized compilation flags: `-O3 -fjump-tables -ftree-switch-conversion`

## Architecture Overview

### Core Components
1. **Main Application (main.cpp:364-428):** Entry point that initializes file browser, loads ROM, sets up emulator, and starts main game loop
2. **Emulator Core (gnuboy/):** Complete Game Boy emulator implementation including CPU, LCD, sound, and hardware emulation
3. **VMUPro Integration:** Direct integration with VMUPro SDK for display, audio, input, and file system operations
4. **Menu System (main.cpp:86-336):** Pause menu with save/load, settings (brightness/volume/palette), and quit options

### Key Architecture Decisions
- **Double-buffered rendering:** Uses VMUPro's double buffer system for smooth 60fps gameplay
- **Real-time audio streaming:** 44.1kHz audio via VMUPro's audio callback system
- **File-based ROM loading:** ROMs loaded from `/sdcard/roms/GameBoy/` with `.gb` extension filter
- **SRAM persistence:** Battery-backed saves stored as `.sram` files alongside ROMs
- **Frame timing:** Precise 60fps timing with microsecond-level frame pacing and skip-frame support

### Input Mapping
- D-Pad → Game Boy D-Pad
- Btn_A → Game Boy A
- Btn_B → Game Boy B  
- Btn_Power → Game Boy SELECT
- Btn_Mode → Game Boy START
- Btn_Bottom → Pause menu toggle

### Memory Architecture
- **Video buffer:** Back buffer from VMUPro for frame rendering (240x240 → 160x144 GB screen)
- **Audio buffer:** 736 samples (uint32_t) for stereo 16-bit audio
- **Pause buffer:** 115200 bytes for background image during menu
- **ROM banking:** Dynamic loading system for large ROMs with bank swapping

## Application Metadata
Applications require metadata.json with these fields:
- `app_name`: "Gnuboy" 
- `app_mode`: 2 (emulator mode)
- `app_entry_point`: "app_main"
- `icon_transparency`: true
- BMP icon file required for packaging

## File Structure
- `src/main/main.cpp` - Main application logic and menu system
- `src/main/gnuboy/` - Complete Game Boy emulator core
- `src/main/CMakeLists.txt` - Component build configuration
- `src/CMakeLists.txt` - Project-level build configuration  
- `src/pack.sh` - Packaging script wrapper
- `src/send.sh` - Device deployment script
- `vmupro-sdk/` - VMUPro SDK submodule dependency

## Development Workflow
1. Make changes to source files in src/main/
2. Build with `idf.py build` from src/ directory
3. Package with `./pack.sh` 
4. Deploy to device with `./send.sh <port>`
5. ROM files go in `/sdcard/roms/GameBoy/` on device
6. Save files automatically created as `.sram` files

## Emulator Features
- **ROM Support:** Standard Game Boy (.gb) ROMs with MBC1/2/3/5 support
- **Save States:** SRAM battery backup simulation with persistence
- **Display:** Multiple Game Boy color palettes (37 total including DMG, MGB, CGB, SGB)
- **Audio:** Full Game Boy sound channels with real-time streaming
- **Controls:** Complete button mapping with pause menu overlay
- **Performance:** Optimized for 60fps with frame skip when needed

## VMUPro SDK Integration
The emulator leverages VMUPro SDK APIs extensively:
- `vmupro_display_*` functions for graphics and double buffering
- `vmupro_audio_*` functions for real-time audio streaming  
- `vmupro_btn_*` functions for input handling
- `vmupro_file_*` functions for ROM and save file management
- `vmupro_emubrowser_*` functions for game selection interface