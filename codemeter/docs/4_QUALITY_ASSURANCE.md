# Quality Assurance

**Project:** M5Clawd
**Last Updated:** 2026-05-14

---

## Test Strategy

**Approach:** **Hybrid** — automated unit tests for pure-logic modules (header parsing, state transitions); manual smoke tests on real hardware for everything that touches the radio, the display, or persistent storage.

**Coverage target (MVP):** ≥ 60% line coverage on pure-logic modules. No hard target on hardware-touching modules — they're validated end-to-end.

**CI/CD Integration:** Phase 2+. GitHub Actions runs `arduino-cli compile` and the host unit tests on PRs. No simulator (Wokwi support deferred).

**Why this approach:** Embedded firmware QA is mostly about real-hardware behavior. Mocking `WiFiClientSecure` and the `M5.Lcd` panel pays back less than running smoke tests on the actual device, especially given a single contributor and one M5Stack on hand.

---

## Test Types

### Unit Tests

**Framework:** Plain C++ test files compiled with the **host `g++`** — no Arduino runtime. Pure-logic modules (header parsing, formatting, the state machine) are written so they don't depend on `M5Stack.h` / `WiFi`, which lets them compile and run on a laptop.
**Location:** `m5clawd/test/` — a `run.sh` (or `Makefile`) that `g++ -std=c++17`-compiles each `*_test.cpp` against the pure-logic source and runs it.
**Coverage target:** ≥ 60% on pure modules

> The original plan used PlatformIO's Unity test env. Since [ADR 001](decisions/001-tech-stack-selection.md) moved us to the Arduino / `arduino-cli` toolchain (no native test env), we test pure logic by host-compiling it directly. The pure-logic functions must therefore stay free of Arduino headers — keep them in `.h`/`.cpp` pairs that include only `<cstdint>`, `<string>`, etc.

**What gets unit tested:**

- `parse_anthropic_headers()` — given a fake header map, returns a correct `UsageData`
- `format_reset_countdown()` — `7320 seconds` → `"2h 2m"`
- `usage_state_machine` — given a sequence of poll results, asserts UI state transitions
- `secret_redactor()` — `"sk-ant-abc123xyz"` → `"sk-ant-***"`

**Run:**

```bash
m5clawd/test/run.sh        # host-compiles + runs every *_test.cpp
```

**What does NOT get unit tested:**

- Anything touching `M5Stack.h` / `M5.Lcd` (no test doubles — real device only)
- WiFi connection logic (real device only)
- TLS handshake to Anthropic (real device only, see Integration below)

### Integration Tests

**Framework:** Manual + scripted, using `arduino-cli monitor` serial output
**Location:** `m5clawd/test/integration/` — smoke-test recipes (Phase 2)
**Coverage target:** All five `UsageData::Status` transitions reachable from external triggers

**Smoke test scenarios:**

1. **Fresh flash → portal mode** — full-erase upload, then `./flash.sh`. Power cycle. AP SSID appears within 30s. Connect phone. Captive portal opens. Submit a garbage API key. Should reject with the `auth!` badge.
2. **Happy path** — Submit a valid API key. Device reboots. Within 90s, the Usage screen shows real numbers.
3. **WiFi drop** — Power down the router. Within one poll interval, status shifts to `wifi-`. Numbers persist from the last good poll. Power the router back up. Within two poll intervals, status restores to `wifi`.
4. **API key rotation** — Long-press C. Confirm the reset prompt on screen. Device reboots into the portal. Submit a new key. Verify polling resumes.
5. **Reflash without erase** — `./flash.sh` (no erase). Verify creds survive and polling resumes without re-onboarding.

**Run:** Manually, per release.

### End-to-End Tests

Not applicable in the web sense. The "E2E" of this project **is** the integration smoke tests above — running on real hardware against real Anthropic API.

### Performance Tests

**Targets:**

| Metric                              | Target          | Method                                |
| ----------------------------------- | --------------- | ------------------------------------- |
| Cold-boot to first usable screen    | < 3 s           | Wall clock from USB plug              |
| Provisioning → first poll           | < 30 s          | Wall clock from form submit           |
| Single poll latency (TLS + parse)   | < 5 s p95       | Serial timestamps                     |
| Free heap after 24h uptime          | > 100 KB        | Periodic `ESP.getFreeHeap()` print    |
| WiFi reconnect after AP outage      | < 30 s          | Manual trigger                        |

**Run:** Per release, by enabling timing prints in `m5clawd.ino` and observing serial output.

### Regression Suite

**Frequency:** Before every merge to `main` (post-Phase 1)
**Duration:** ~15 minutes (manual smoke tests + automated unit run)

**Run:**

```bash
m5clawd/test/run.sh                  # host unit tests
arduino-cli compile --fqbn esp32:esp32:m5stack-core-esp32 m5clawd   # build check
# Then: physically run smoke tests 1-5 above
```

---

## Quality Gates

### Pre-Commit

- [ ] `arduino-cli compile` builds clean (no warnings)
- [ ] `m5clawd/test/run.sh` passes (host unit tests)
- [ ] No secrets in the diff (`git diff --cached | grep -E 'sk-ant-[A-Za-z0-9]{40,}'` empty)

### Pull Request

- [ ] All quality gates above
- [ ] At least one smoke test from §Integration run on real hardware, with serial log pasted in PR body
- [ ] HANDOFF_NOTES.md updated
- [ ] CHANGELOG.md updated (when we have one — Phase 2)

### Pre-Release

- [ ] All 5 smoke-test scenarios pass on real hardware
- [ ] 24h uptime test: leave device running, check heap, uptime, and poll-success rate the next day
- [ ] Verify reflash-without-erase preserves creds
- [ ] Confirm captive portal works on iOS + Android (one real device each)

---

## Test Coverage

Pure-logic modules live in Arduino-header-free `.h`/`.cpp` pairs so the host tests can compile them; the `.ino` tabs are device-only.

| Module                          | Unit    | Integration       | Real-device only |
| -------------------------------- | ------- | ----------------- | ---------------- |
| `parse_headers.{h,cpp}` (pure)   | ~80%    | n/a               | no               |
| `state_machine.{h,cpp}` (pure)   | ~80%    | n/a               | no               |
| `format_helpers.{h,cpp}` (pure)  | ~90%    | n/a               | no               |
| `wifi_portal.ino`                | n/a     | scenario 1        | yes              |
| `poller.ino`                     | n/a     | scenarios 2, 3, 5 | yes              |
| `ui.ino`                         | n/a     | all               | yes              |
| button handling (`m5clawd.ino`)  | n/a     | scenario 4        | yes              |
| `secrets_store.ino`              | partial | scenario 5        | partial          |

**Overall (pure logic):** target 60%. Hardware modules verified via real-device runs.

---

## Bug Tracking

**Tool:** GitHub Issues (once the project has a public repo; until then, `HANDOFF_NOTES.md` "Known Issues" table).
**Severity Levels:** CRITICAL · HIGH · MEDIUM · LOW

### Bug workflow

```text
[Discovered] → [Triaged in HANDOFF_NOTES or Issues] → [In Phase TASKS] → [Fixed] → [Verified on hardware] → [Closed]
```

### SLA (single-contributor side project — informational only)

- **CRITICAL** (device bricks, secrets leak, can't onboard): next session
- **HIGH** (poll loop unreliable, UI corrupted): within a phase
- **MEDIUM** (cosmetic regression, suboptimal UX): next phase
- **LOW** (nice-to-have): backlog

---

## Testing Tools

| Category          | Tool                          | Purpose                                       |
| ----------------- | ----------------------------- | --------------------------------------------- |
| Unit testing      | host `g++` (`test/run.sh`)    | C++ unit tests for pure-logic modules         |
| Static analysis   | `cppcheck` (optional, manual) | Bug pattern detection                         |
| Build             | `arduino-cli compile`         | Compile check; warnings treated as review-worthy |
| Flash             | `arduino-cli upload` (`flash.sh`) | Firmware + SPIFFS over USB-C               |
| Real-device debug | `arduino-cli monitor`         | Serial log @ 115200                           |
| Secret scan       | `git grep` (pre-commit + CI)  | Prevent `sk-ant-…` in git                     |

---

## CI/CD Integration

**Phase 2 onwards.** Planned GitHub Actions workflow:

```yaml
# .github/workflows/ci.yml (sketch)
on: [pull_request, push]
jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: arduino/setup-arduino-cli@v2
      - run: |
          arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
          arduino-cli core install esp32:esp32@1.0.4 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
          arduino-cli lib install "M5Stack@0.3.1" "ArduinoJson@6.17.3"
      - run: arduino-cli compile --fqbn esp32:esp32:m5stack-core-esp32 m5clawd
      - run: m5clawd/test/run.sh
      - run: |
          if git grep -E 'sk-ant-[A-Za-z0-9]{40,}' -- ':!docs'; then
            echo "API key leaked into source"; exit 1
          fi
```

**Failed build policy:** Block merge.

---

## Manual Testing

### Test environments

- **Development:** Real hardware on USB, serial monitor open
- **Staging:** Same hardware on a USB charger (battery powered or via charger), used for unattended uptime checks
- **Production:** Same device — it's a single-device deployment

### Smoke tests

(See §Integration Tests above — scenarios 1–5.)

### Regression checklist

- [ ] Splash renders on boot
- [ ] WiFiManager portal pops on first boot
- [ ] Captive portal accepts WiFi + API key, persists, reboots
- [ ] Usage screen renders within 30s of reboot post-portal
- [ ] Long-press button C resets NVS with on-screen confirmation
- [ ] Power off / on preserves last-good usage values

---

## Known Issues

| ID    | Severity | Component        | Status   | Notes                                                                |
| ----- | -------- | ---------------- | -------- | -------------------------------------------------------------------- |
| KI-01 | LOW      | Captive portal   | Open     | iOS DNS-over-HTTPS may suppress auto-pop. Manual URL is documented.  |
| KI-02 | MED      | NVS storage      | Open     | API key is plaintext at rest. See ADR 005 for hardening plan.        |
| KI-03 | LOW      | Documentation    | Open     | Anthropic CA pinning may break if upstream rotates roots.            |

---

## Security Audit Cadence

Per release:

1. `git grep -E 'sk-ant-' $(git ls-files)` returns nothing
2. No new `Serial.print` calls that touch the secret
3. Review `WiFiClientSecure::setCACert` for stale roots (annual at minimum)

---

**QA Lead:** Kevin Brice
**Next Audit:** End of Phase 1
