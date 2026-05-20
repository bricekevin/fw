// ota.h — Over-the-air firmware updates via GitHub Releases (ADR 011).
//
// Polls api.github.com/repos/bricekevin/fw/releases/latest, parses the tag
// and the firmware.bin asset URL, exposes the resulting state to the UI.
// The user starts the install with a 5 s hold on Button B while the Status
// screen shows "update available"; ESP32's two-slot OTA + auto-rollback is
// the safety net.
#pragma once

#include <Arduino.h>

struct OtaState {
  enum Phase {
    IDLE = 0,         // never checked, or check long ago
    CHECKING,         // GitHub API request in flight
    UP_TO_DATE,       // checked, no newer version
    AVAILABLE,        // newer release found, awaiting user
    DOWNLOADING,      // streaming firmware.bin into the inactive slot
    INSTALLING,       // finalising; about to reboot
    FAILED            // last attempt failed (see lastError)
  };
  Phase    phase             = IDLE;
  char     latestVersion[24] = "";   // tag_name from GitHub
  uint8_t  downloadPct       = 0;    // 0..100 during DOWNLOADING
  uint32_t lastCheckMs       = 0;    // millis() of the last completed check
  char     lastError[40]     = "";   // human-readable on FAILED
};

extern OtaState g_ota;

void        ota_check_rollback_on_boot(); // call ASAP in setup() (see ota.ino)
void        ota_begin();              // once from setup() (marks image valid)
void        ota_tick();               // every loop() iteration
void        ota_check_now();          // force a poll (boot, manual trigger)
void        ota_apply_update_now();   // begin streaming the available asset
const char *ota_phase_str();          // human-readable for the UI
