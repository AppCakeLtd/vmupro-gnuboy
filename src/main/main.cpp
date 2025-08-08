#include <string.h>
#include <vmupro_sdk.h>

#include <gnuboy.h>

static const char *kLogGBCEmu = "[GNUBOY]";
static char *launchfile       = nullptr;

enum class EmulatorMenuState { EMULATOR_RUNNING, EMULATOR_MENU, EMULATOR_CONTEXT_MENU, EMULATOR_SETTINGS_MENU };

typedef enum ContextMenuEntryType {
  MENU_ACTION,
  MENU_OPTION_VOLUME,
  MENU_OPTION_BRIGHTNESS,
  MENU_OPTION_PALETTE,
  MENU_OPTION_SCALING,
  MENU_OPTION_STATE_SLOT,
  MENU_OPTION_NONE
};

typedef struct ContextMenuEntry_s {
  const char *title;
  bool enabled              = true;
  ContextMenuEntryType type = MENU_ACTION;
} ContextMenuEntry;

const ContextMenuEntry emuContextEntries[5] = {
    {.title = "Save & Continue", .enabled = true, .type = MENU_ACTION},
    {.title = "Load Game", .enabled = true, .type = MENU_ACTION},
    {.title = "Restart", .enabled = true, .type = MENU_ACTION},
    {.title = "Options", .enabled = true, .type = MENU_ACTION},
    {.title = "Quit", .enabled = true, .type = MENU_ACTION},
};

const ContextMenuEntry emuContextOptionEntries[5] = {
    {.title = "Volume", .enabled = true, .type = MENU_OPTION_VOLUME},
    {.title = "Brightness", .enabled = true, .type = MENU_OPTION_BRIGHTNESS},
    {.title = "Palette", .enabled = true, .type = MENU_OPTION_PALETTE},
    {.title = "State Slot", .enabled = true, .type = MENU_OPTION_STATE_SLOT},
    {.title = "", .enabled = false, .type = MENU_OPTION_NONE},
};

static EmulatorMenuState currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;

static uint8_t *video_back_buffer   = nullptr;
volatile uint32_t *gbc_audio_buffer = nullptr;
static uint8_t *pauseBuffer         = nullptr;

static int gbContextSelectionIndex = 0;
static int gbCurrentPaletteIndex   = 0;
static int renderFrame             = 1;
static int frame_counter           = 0;
static uint32_t num_frames         = 0;
static uint64_t frame_time         = 0.0f;
static uint64_t frame_time_total   = 0.0f;
static uint64_t frame_time_max     = 0.0f;
static uint64_t frame_time_min     = 0.0f;
static float frame_time_avg        = 0.0f;

static bool inOptionsMenu = false;

static bool emuRunning = true;

static void update_frame_time(uint64_t ftime) {
  num_frames++;
  frame_time       = ftime;
  frame_time_total = frame_time_total + frame_time;
}

static void reset_frame_time() {
  num_frames       = 0;
  frame_time       = 0;
  frame_time_total = 0;
  frame_time_max   = 0;
  frame_time_min   = 1000000;  // some large number
  frame_time_avg   = 0.0f;
}

static float get_fps() {
  if (frame_time_total == 0) {
    return 0.0f;
  }
  return num_frames / (frame_time_total / 1e6f);
}

void Tick() {
  static constexpr uint64_t max_frame_time = 1000000 / 60.0f;  // microseconds per frame
  vmupro_display_clear(VMUPRO_COLOR_BLACK);
  vmupro_display_refresh();

  int64_t next_frame_time = vmupro_get_time_us();
  int64_t lastTime        = next_frame_time;
  int64_t accumulated_us  = 0;
  int padold              = 0;

  while (emuRunning) {
    int64_t currentTime = vmupro_get_time_us();
    float fps_now       = get_fps();

    vmupro_btn_read();
    if (currentEmulatorState == EmulatorMenuState::EMULATOR_MENU) {
      vmupro_display_clear(VMUPRO_COLOR_BLACK);
      // vmupro_blit_buffer_dithered(pauseBuffer, 0, 0, 240, 240, 2);
      vmupro_blit_buffer_blended(pauseBuffer, 0, 0, 240, 240, 150);
      // We'll only use our front buffer for this to simplify things
      int startY = 50;
      vmupro_draw_fill_rect(40, 37, 200, 170, VMUPRO_COLOR_NAVY);

      vmupro_set_font(VMUPRO_FONT_OPEN_SANS_15x18);
      for (int x = 0; x < 5; ++x) {
        uint16_t fgColor = gbContextSelectionIndex == x ? VMUPRO_COLOR_NAVY : VMUPRO_COLOR_WHITE;
        uint16_t bgColor = gbContextSelectionIndex == x ? VMUPRO_COLOR_WHITE : VMUPRO_COLOR_NAVY;
        if (gbContextSelectionIndex == x) {
          vmupro_draw_fill_rect(50, startY + (x * 22), 190, (startY + (x * 22) + 20), VMUPRO_COLOR_WHITE);
        }
        if (inOptionsMenu) {
          if (!emuContextOptionEntries[x].enabled) fgColor = VMUPRO_COLOR_GREY;
          vmupro_draw_text(emuContextOptionEntries[x].title, 60, startY + (x * 22), fgColor, bgColor);
          // For options, we need to draw the current option value right aligned within the selection boundary
          switch (emuContextOptionEntries[x].type) {
            case MENU_OPTION_BRIGHTNESS: {
              char currentBrightness[5] = "0%";
              vmupro_snprintf(currentBrightness, 6, "%d%%", (vmupro_get_global_brightness() * 10) + 10);
              int tlen = vmupro_calc_text_length(currentBrightness);
              vmupro_draw_text(currentBrightness, 190 - tlen - 5, startY + (x * 22), fgColor, bgColor);
            } break;
            case MENU_OPTION_VOLUME: {
              char currentVolume[5] = "0%";
              vmupro_snprintf(currentVolume, 6, "%d%%", (vmupro_get_global_volume() * 10) + 10);
              int tlen = vmupro_calc_text_length(currentVolume);
              vmupro_draw_text(currentVolume, 190 - tlen - 5, startY + (x * 22), fgColor, bgColor);
            } break;
            case MENU_OPTION_PALETTE: {
              int tlen = vmupro_calc_text_length(PaletteNames[gbCurrentPaletteIndex]);
              vmupro_draw_text(
                  PaletteNames[gbCurrentPaletteIndex], 190 - tlen - 5, startY + (x * 22), fgColor, bgColor
              );
            } break;
            default:
              break;
          }
        }
        else {
          if (!emuContextEntries[x].enabled) fgColor = VMUPRO_COLOR_GREY;
          vmupro_draw_text(emuContextEntries[x].title, 60, startY + (x * 22), fgColor, bgColor);
        }
      }
      vmupro_display_refresh();

      if (vmupro_btn_pressed(Btn_B)) {
        // Exit the context menu, resume execution
        // Reset our frame counters otherwise the emulation
        // will run at full blast :)
        if (inOptionsMenu) {
          inOptionsMenu = false;
        }
        else {
          if (gbContextSelectionIndex != 4) {
            vmupro_resume_double_buffer_renderer();
          }
          reset_frame_time();
          lastTime             = vmupro_get_time_us();
          accumulated_us       = 0;
          currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;
        }
      }
      else if (vmupro_btn_pressed(Btn_A) && !inOptionsMenu) {
        // Get the selection index. What are we supposed to do?
        if (gbContextSelectionIndex == 0) {
          vmupro_resume_double_buffer_renderer();
          // Save in both cases
          // savaStateHandler((const char *)launchfile);

          // Close the modal
          reset_frame_time();
          lastTime             = vmupro_get_time_us();
          accumulated_us       = 0;
          currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;
        }
        else if (gbContextSelectionIndex == 1) {
          vmupro_resume_double_buffer_renderer();
          // loadStateHandler((const char *)launchfile);

          // Close the modal
          reset_frame_time();
          lastTime             = vmupro_get_time_us();
          accumulated_us       = 0;
          currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;
        }
        else if (gbContextSelectionIndex == 2) {  // Reset
          vmupro_resume_double_buffer_renderer();
          reset_frame_time();
          lastTime       = vmupro_get_time_us();
          accumulated_us = 0;
          gnuboy_reset(true);
          currentEmulatorState = EmulatorMenuState::EMULATOR_RUNNING;
        }
        else if (gbContextSelectionIndex == 3) {  // Options
          // let's change a flag, let the rendered in the next iteration re-render the menu
          inOptionsMenu = true;
        }
        else if (gbContextSelectionIndex == 4) {  // Quit
          emuRunning = false;
        }
      }
      else if (vmupro_btn_pressed(DPad_Down)) {
        if (gbContextSelectionIndex == 4)
          gbContextSelectionIndex = 0;
        else
          gbContextSelectionIndex++;
      }
      else if (vmupro_btn_pressed(DPad_Up)) {
        if (gbContextSelectionIndex == 0)
          gbContextSelectionIndex = 4;
        else
          gbContextSelectionIndex--;
      }
      else if ((vmupro_btn_pressed(DPad_Right) || vmupro_btn_pressed(DPad_Left)) && inOptionsMenu) {
        // Adjust the setting
        switch (emuContextOptionEntries[gbContextSelectionIndex].type) {
          case MENU_OPTION_BRIGHTNESS: {
            uint8_t currentBrightness = vmupro_get_global_brightness();
            uint8_t newBrightness     = 1;
            if (vmupro_btn_pressed(DPad_Right)) {
              newBrightness = currentBrightness < 10 ? currentBrightness + 1 : 10;
            }
            else {
              newBrightness = currentBrightness > 0 ? currentBrightness - 1 : 0;
            }
            // settings->brightness = newBrightness;
            vmupro_set_global_brightness(newBrightness);
            // OLEDDisplay::getInstance()->setBrightness(newBrightness, true, false);
          } break;
          case MENU_OPTION_VOLUME: {
            uint8_t currentVolume = vmupro_get_global_volume();
            uint8_t newVolume     = 1;
            if (vmupro_btn_pressed(DPad_Right)) {
              newVolume = currentVolume < 10 ? currentVolume + 1 : 10;
            }
            else {
              newVolume = currentVolume > 0 ? currentVolume - 1 : 0;
            }
            vmupro_set_global_volume(newVolume);
          } break;
          case MENU_OPTION_PALETTE:
            break;
          default:
            break;
        }
      }
    }
    else {
      int pad = 0;
      pad |= vmupro_btn_held(DPad_Up) ? GB_PAD_UP : 0;
      pad |= vmupro_btn_held(DPad_Right) ? GB_PAD_RIGHT : 0;
      pad |= vmupro_btn_held(DPad_Down) ? GB_PAD_DOWN : 0;
      pad |= vmupro_btn_held(DPad_Left) ? GB_PAD_LEFT : 0;
      pad |= vmupro_btn_held(Btn_Power) ? GB_PAD_SELECT : 0;
      pad |= vmupro_btn_held(Btn_Mode) ? GB_PAD_START : 0;
      pad |= vmupro_btn_held(Btn_A) ? GB_PAD_A : 0;
      pad |= vmupro_btn_held(Btn_B) ? GB_PAD_B : 0;

      if (pad != padold) {
        gnuboy_set_pad(pad);  // costly call apparently tbc
        padold = pad;
      }

      if (vmupro_btn_pressed(Btn_Bottom)) {
        // Copy the rendered framebuffer to our pause buffer
        vmupro_delay_ms(16);

        // get the last rendered buffer
        if (vmupro_get_last_blitted_fb_side() == 0) {
          memcpy(pauseBuffer, vmupro_get_front_fb(), 115200);
        }
        else {
          memcpy(pauseBuffer, vmupro_get_back_fb(), 115200);
        }
        vmupro_pause_double_buffer_renderer();

        // Set the state to show the overlay
        currentEmulatorState = EmulatorMenuState::EMULATOR_MENU;

        // Delay by one frame to allow the display to finish refreshing
        vmupro_delay_ms(16);
        continue;
      }

      if (renderFrame) {
        video_back_buffer = vmupro_get_back_buffer();
        gnuboy_set_framebuffer(video_back_buffer);
        gnuboy_run(true);
        vmupro_push_double_buffer_frame();
      }
      else {
        gnuboy_run(false);
        renderFrame = 1;
      }

      ++frame_counter;

      int64_t elapsed_us = vmupro_get_time_us() - currentTime;
      int64_t sleep_us   = max_frame_time - elapsed_us + accumulated_us;

      vmupro_log(
          VMUPRO_LOG_INFO,
          kLogGBCEmu,
          "loop %i, fps: %.2f, elapsed: %lli, sleep: %lli",
          frame_counter,
          fps_now,
          elapsed_us,
          sleep_us
      );

      if (sleep_us > 360) {
        vmupro_delay_us(sleep_us - 360);  // subtract 360 micros for jitter
        accumulated_us = 0;
      }
      else if (sleep_us < 0) {
        renderFrame    = 0;
        accumulated_us = sleep_us;
      }

      update_frame_time(currentTime - lastTime);
      lastTime = currentTime;
    }
  }
}

void Exit() {
  gnuboy_free_rom();
  gnuboy_free_mem();
  gnuboy_free_bios();
}

void audio_callback(void *buffer, size_t length) {
  int16_t *buff16 = (int16_t *)buffer;
  vmupro_audio_add_stream_samples(buff16, length, vmupro_stereo_mode_t::VMUPRO_AUDIO_STEREO, true);
}

void app_main(void) {
  vmupro_log(VMUPRO_LOG_INFO, kLogGBCEmu, "Starting Gnuboy");
  vmupro_emubrowser_settings_t emuSettings = {
      .title = "Gnuboy Emulator", .rootPath = "/sdcard/roms/GameBoy", .filterExtension = ".gb"
  };
  vmupro_emubrowser_init(emuSettings);

  launchfile = (char *)malloc(512);
  memset(launchfile, 0x00, 512);
  vmupro_emubrowser_render_contents(launchfile);
  if (strlen(launchfile) == 0) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogGBCEmu, "We somehow exited the browser with no file to show!");
    return;
  }

  char launchPath[512 + 22];
  vmupro_snprintf(launchPath, (512 + 22), "/sdcard/roms/GameBoy/%s", launchfile);

  // Initialise RAM space
  vmupro_start_double_buffer_renderer();
  video_back_buffer = vmupro_get_back_buffer();

  gbc_audio_buffer = (uint32_t *)malloc(736 * sizeof(uint32_t));
  if (!gbc_audio_buffer) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogGBCEmu, "Failed allocating sound buffer!");
    return;
  }

  pauseBuffer = (uint8_t *)malloc(115200);
  if (!pauseBuffer) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogGBCEmu, "Failed allocating pause buffer!");
    return;
  }

  // Initialise gnuboy
  if (gnuboy_init(44100, GB_AUDIO_STEREO_S16, GB_PIXEL_PALETTED_BE, NULL, audio_callback) < 0) {
    vmupro_log(VMUPRO_LOG_ERROR, kLogGBCEmu, "Emulator initialisation failed!");
    return;
  }

  gnuboy_set_framebuffer(video_back_buffer);
  gnuboy_set_soundbuffer((void *)gbc_audio_buffer, 736);

  gnuboy_load_rom_file(launchPath);
  gnuboy_set_palette(GB_PALETTE_DMG);

  gnuboy_set_video_params(0, 12, 1);

  gnuboy_reset(true);

  // char sramfile[MAX_PATH_LEN];
  // sprintf(sramfile, "/sdcard/roms/%s/%s.sram", consolePathName, launchfile);
  // if (SDFS::checkFileExists(sramfile)) {
  //   gnuboy_load_sram(sramfile);
  // }

  vmupro_audio_start_listen_mode();

  Tick();

  Exit();

  vmupro_log(VMUPRO_LOG_INFO, kLogGBCEmu, "Game Boy Emulator initialisation done");
}
