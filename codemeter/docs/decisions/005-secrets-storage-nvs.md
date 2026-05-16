# ADR 005: Secrets Storage in NVS

**Date:** 2026-05-14
**Status:** Accepted — revised by [ADR 008](008-refresh-token-storage.md)
**Deciders:** Kevin Brice
**Related:** [002-wifi-vs-ble-daemon](002-wifi-vs-ble-daemon.md), [003-onboarding-via-wifimanager](003-onboarding-via-wifimanager.md), [008-refresh-token-storage](008-refresh-token-storage.md)

---

> **Revised (2026-05-15).** Phase 3 replaces the single `anthropic_key` API key
> with an OAuth credential record (access token + **refresh token** + expiry).
> [ADR 008](008-refresh-token-storage.md) extends the NVS layout below and
> re-decides the encryption trade-off for the higher-value refresh token. The
> plaintext-NVS storage mechanism and the encryption *deferral* are unchanged;
> ADR 008's compensating controls (revocation guidance) are new.

---

## Context

The device needs to persist three pieces of secret-ish data across reboots:

1. WiFi SSID (not really secret, but pairs with the password)
2. WiFi password
3. Anthropic API key (`sk-ant-…`) — the actual secret

We need a storage choice that:

- Survives power loss
- Survives firmware reflashes when desired, but is wiped on user request (reset gesture)
- Is **not** in the project's git tree (the cardinal sin of `secrets.h.committed`)
- Is feasible on the M5Stack Core Basic without external storage

---

## Decision

**We will store all three secrets in ESP32's NVS (Non-Volatile Storage) flash partition, using the Arduino `Preferences.h` wrapper.**

Layout:

- **WiFi creds:** Managed by `WiFiManager` (which uses its own NVS slot via `WiFi.persistent(true)` + `WiFi.begin()` internals). We don't touch these directly.
- **API key:** Our own `Preferences` namespace `"m5clawd"`, key `"anthropic_key"`. Written once during captive-portal save, read at boot.
- **Reset behavior:** Holding **button C for 5 seconds** clears both the WiFiManager slot (`wifiManager.resetSettings()`) and our `Preferences` namespace (`prefs.clear()`).

**Operational rules:**

- The API key is **never logged**. Serial monitor must not echo it. Any error involving the key prints `sk-ant-***` style redaction.
- Captive-portal HTML form field is `type="password"` so the browser hides input.
- We do **not** enable NVS encryption in MVP. This is a known soft spot — see ADR 002 consequences and "Future Hardening" below.

---

## Alternatives Considered

### Option 1: SPIFFS / LittleFS file

**Description:** Mount a flash filesystem; store secrets as a JSON file in `/secrets.json`.

**Pros:**
- Easier to inspect/debug (`espefuse.py read_flash` and grep).
- Files are familiar.

**Cons:**
- The "easier to inspect" cuts both ways — it's also easier to extract from a stolen device.
- Mounting a filesystem just for one tiny config file is overkill.
- Wear-leveling story for tiny config writes is slightly worse than NVS.

**Rejected because:** No advantage over NVS for this use case.

### Option 2: SD card (Core Basic has a slot)

**Description:** Write `secrets.json` to the microSD card.

**Pros:**
- User can remove the SD before handing off the device — physical control.
- Easy to back up.

**Cons:**
- Requires the user to insert an SD card to use the device. Friction.
- If the SD pops out mid-operation, secrets vanish unpredictably.
- Many users won't have a spare microSD lying around.

**Rejected because:** Requiring SD insertion contradicts "easy to setup once cloned".

### Option 3: NVS with encryption enabled

**Description:** Same as the chosen option but with NVS encryption keys stored in the eFuse partition.

**Pros:**
- Stolen-device attack requires extracting eFuse, which is hardware-attack-grade.
- Best-in-class for ESP32.

**Cons:**
- Adds setup complexity: encryption keys must be provisioned at flash time.
- Breaks the "anyone can `./flash.sh` from a clean clone" first-time-setup flow.
- The user's threat model is "if someone physically has my desk device, I'd rotate the key" — not nation-state.

**Deferred** (not rejected): We plan to enable this in a hardening phase once MVP is stable. For now, plaintext NVS is the right level of paranoia.

### Option 4: Hardcoded `secrets.h` (git-ignored)

**Description:** User edits `src/secrets.h` (which is `.gitignore`'d) before flashing.

**Pros:**
- Simplest possible.

**Cons:**
- See ADR 003 — entire point of WiFiManager was to avoid this.

**Rejected because:** Contradicts ADR 003.

---

## Consequences

### Positive

- **Survives firmware updates** unless we explicitly clear the NVS partition during flash (a plain `arduino-cli upload` does not — only a full-erase upload does).
- **Single API to manage:** `Preferences.h` is part of Arduino-ESP32, no extra deps.
- **Wear-leveled automatically** by the NVS subsystem.

### Negative

- **Plaintext on flash.** Anyone with physical access to the device and a USB cable can `esptool.py read_flash` the NVS partition and recover the API key. Mitigated by:
  - Recommending a **scoped Anthropic project key** with low limits.
  - Documenting NVS-encryption upgrade path.
  - Reset gesture for fast revocation.
- **Re-flashing firmware does _not_ wipe secrets by default.** This is intentional (good for upgrades) but surprising. Documented in README.
- **A full-erase upload** (`arduino-cli upload -e ...`) wipes everything — including secrets. We mention this in the troubleshooting section.

### Neutral

- **Default Arduino-ESP32 partition layout** has a 24KB NVS partition. Plenty for our use case.

---

## Implementation

**Timeline:** Phase 1 (alongside WiFiManager integration)

**Steps:**

1. `#include <Preferences.h>` in the boot path
2. `prefs.begin("m5clawd", false)` at startup
3. On captive-portal save: `prefs.putString("anthropic_key", key)`
4. At every poll: `prefs.getString("anthropic_key", "")` (cache in RAM after first read)
5. On long-press reset: `prefs.clear()` + `wifiManager.resetSettings()` + `ESP.restart()`

**Dependencies:**

- `Preferences.h` (Arduino-ESP32 stdlib)

**Risks:**

- **API key written to flash on every onboarding** — but onboarding is rare, so wear is not a concern.
- **A future feature might want a longer-lived NVS namespace** (e.g., user-customizable poll interval). Plan now: keep the `"m5clawd"` namespace and add keys like `"poll_interval_s"` alongside `"anthropic_key"`.

---

## Future Hardening

When MVP is stable, consider:

1. **NVS encryption:** Provision per-device encryption key via `idf.py secure_boot_*` flow. Documented in `4_QUALITY_ASSURANCE.md`.
2. **Token rotation reminder:** Show a UI hint after 90 days suggesting key rotation.
3. **Optional 2-factor:** Captive-portal could require a token PIN printed on the device case before saving.

---

## Validation

**How will we know this was the right call?**

- A user can re-flash firmware without re-onboarding (creds survive).
- The reset gesture reliably wipes both WiFi and API key.
- API key never appears in serial logs, git diffs, or screenshots.

**Review Date:** End of Phase 2

---

## References

- [Arduino-ESP32 Preferences docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html)
- [NVS encryption guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_encryption.html)
- [Anthropic API key best practices](https://docs.anthropic.com/en/api/getting-started#api-keys)

---

**Last Updated:** 2026-05-14
**Updated By:** Kevin Brice
