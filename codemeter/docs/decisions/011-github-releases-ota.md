# ADR 011 — Firmware updates via GitHub Releases

**Status:** Accepted · 2026-05-19
**Supersedes:** —
**Superseded by:** —

## Context

Until this session the device had no update path: re-flashing required
opening the case (the M5Stack Core has the USB-C port exposed, but still a
manual deskside operation per unit) and running `./flash.sh` from a checkout.
That works for one dev unit; it does not scale to "I have a couple of
codeMeters around the house" and it is hostile to anyone who is not Kevin.

The M5Stack Core's default Arduino-ESP32 partition table already provides two
1.31 MB OTA slots (`app0`, `app1`) and an `otadata` partition, so the on-chip
plumbing for a swap-and-rollback OTA is already there — what was missing was
a transport, a discovery mechanism, and a UI.

## Decision

Pull firmware from **GitHub Releases** of `bricekevin/fw`. Each release tag
maps to one firmware version; the release carries `firmware.bin` as its
single asset.

- **Discovery:** the device polls `api.github.com/repos/bricekevin/fw/releases/latest`
  on boot (after WiFi is up) and every 6 h thereafter, alongside the existing
  Anthropic poll cadence. The response gives `tag_name` and the
  `browser_download_url` of `firmware.bin`.
- **Version comparison:** `tag_name` is compared to the compiled-in
  `FW_VERSION` constant. Lexicographic semver compare for now (good enough
  while versions are `vX.Y.Z`); upgrade to component-wise compare when
  versions exceed `9` in any field.
- **Apply: automatic** (revised 2026-05-20). When `ota_check_now` finds a
  newer release it calls `ota_apply_update_now` directly — no gesture, no
  countdown. The earlier hold-to-install design was rejected by the operator
  as clunky for a single-user fleet where the operator is also the release
  author. Risks accepted:
  - A bad release auto-propagates; recovery is a USB reflash. Acceptable at
    the current fleet size (≤3 units, same operator).
  - Core 1.0.4 has no `esp_ota_mark_app_valid_cancel_rollback`. **App-level
    rollback added 2026-05-20**: after an OTA install we set a
    `pending`-flag in NVS namespace `m5clawd_ota` and reboot. On the next
    boot, if `esp_reset_reason()` reports a crash (panic / int_wdt /
    task_wdt / wdt / brownout) AND the flag is set, we call
    `esp_ota_set_boot_partition()` on the previous slot and reboot. The
    flag is cleared once the first OTA poll on the new image completes —
    that exercises WiFi, NTP, TLS-with-the-new-CA-pins, and the GitHub
    round-trip, which together are a reasonable "this image works" signal.
    A bad image that boots far enough to clear the flag but is subtly
    broken still needs a USB reflash; not addressed.
  - The boot-time C-hold reset watchdog (m5clawd.ino `boot_reset_watch`)
    still works during `station_connect`, so an image that boots but cannot
    join WiFi remains recoverable without USB.
  Mitigation when the fleet grows: re-add an "any-button-cancel" countdown,
  or add component-wise version compare with an allow-list.
- **Transport:** HTTPS via `WiFiClientSecure` + `HTTPClient`. **TLS pinned**
  to two root CAs covering both legs of the redirect (revised 2026-05-20):
  USERTrust ECC Certification Authority for `api.github.com` (Sectigo
  chain), and ISRG Root X1 for `release-assets.githubusercontent.com`
  (Let's Encrypt chain). Both PEMs are concatenated in `ota_certs.h`;
  `WiFiClientSecure::setCACert(GITHUB_OTA_ROOTS)` lets mbedtls accept either
  as a trust anchor. The poll defers until NTP has sync'd, because
  notBefore checks fail with the boot wall-clock at the epoch.
- **Status surface:** the Status screen gains a "Firmware" block with the
  current version, the available version (if any), the last check time, and
  the current OTA phase (`idle / checking / available / downloading N% /
  installing / failed`).

## Alternatives considered

- **Raw `raw.githubusercontent.com` from a branch.** Simpler but commits
  binaries into the repo (smelly), gives no version metadata, and every push
  becomes a release. Rejected.
- **Self-hosted HTTPS endpoint** (encinitas3d.com or similar). More flexible
  but adds infrastructure to maintain. Rejected for now; can be added as a
  second transport behind the same OTA module if a project needs it later.
- **Fully automatic apply.** Faster propagation but one bad release ships to
  every device unattended. The rollback partition catches outright crashes,
  not subtle regressions. Rejected — hold-to-apply is the right default for a
  hobby-grade fleet.
- **Cryptographic signature on `firmware.bin`.** Correct and standard, but
  needs a key-management story we have no use for at three units. Deferred;
  worth doing before publishing the device to anyone else.

## Consequences

- The repo `bricekevin/fw` is the published source of truth for firmware.
- `FW_VERSION` (in `m5clawd/config.h`) must be bumped before tagging a
  release; the release tag is the version string with no `v` prefix
  variation tolerated (CI assumption — to be enforced when CI exists).
- The Status screen layout grows; documented in `3_DESIGN_SYSTEM.md`.
- The first user of OTA is the developer testing it against this device.
  After it works once end-to-end this becomes the only flashing path needed
  beyond the very first USB flash of a new unit.
