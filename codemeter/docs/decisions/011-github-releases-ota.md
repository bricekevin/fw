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
- **Apply:** found-newer surfaces on the Status screen as a row; **a 5 s hold
  on Button B** while the Status screen is showing starts the download.
  Hold-to-apply (not auto) so a bad release cannot brick a whole desk full
  of devices the night after it ships. The on-chip ESP32 rollback (boot
  validates the new image; a crash in the first run reverts to the previous
  slot) is the safety net underneath that.
- **Transport:** HTTPS via `WiFiClientSecure` + `HTTPClient`. **TLS cert
  validation is deferred** — v1 uses `setInsecure()` and relies on GitHub
  being the only host we ever hit. Embedding GitHub's root CA chain is a
  follow-up; flagged in `4_QUALITY_ASSURANCE.md`.
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
