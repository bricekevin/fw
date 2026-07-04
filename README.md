# fw

Public firmware release artifacts. Each project gets its own tag prefix on
this repo; each release attaches a `firmware.bin` (the app image that the
device's own OTA pulls) and a `firmware-merged.bin` (the full bootloader +
partition table + app, used for initial USB flashes).

The source lives in private per-project repos.

## Flash a brand-new device (web flasher)

Open this page in **Chrome or Edge** on a desktop (WebSerial required) and
follow the on-screen steps:

→ **<https://bricekevin.github.io/fw/>**

No toolchain install, no source clone — the page uses
[esp-web-tools](https://esphome.github.io/esp-web-tools/) to write the
merged firmware directly over USB.

After the first install the device pulls every subsequent release over
WiFi on its own.

## Projects

| Project | Tag prefix | Source repo (private) | Notes |
| --- | --- | --- | --- |
| codeMeter (M5Stack Core) | `codemeter-v*` | [bricekevin/theClaw](https://github.com/bricekevin/theClaw) | Tags `v0.0.2`..`v0.2.0` are pre-convention codeMeter releases |
| Social Meter (M5Stack Core) | `socialmeter-v*` | private | — |
| priceDisplay — crypto/stock ticker (M5Stack Core) | `priceDisplay-v*` | private | Merged image includes SPIFFS (chart PNGs); OTA replaces the app slot only |

### Install pages (web flasher)

- codeMeter — <https://bricekevin.github.io/fw/>
- Social Meter — <https://bricekevin.github.io/fw/socialmeter/>
- priceDisplay — <https://bricekevin.github.io/fw/pricedisplay/>

## OTA discovery (already-flashed device)

The device polls:

    https://api.github.com/repos/bricekevin/fw/releases?per_page=20

…walks the array, keeps tags matching its own project prefix (or the
legacy unprefixed `vX.Y.Z` pattern for codeMeter), and picks the
newest by parsed semver. The chosen release's `firmware.bin` asset is
streamed into the inactive OTA slot. See ADR 011 in the source repo —
TLS-pinned trust chain, app-level rollback, etc.

## Publishing a release

From a build in the project's source repo:

```sh
gh release create codemeter-v0.2.5 firmware.bin firmware-merged.bin \
  --repo bricekevin/fw --target main \
  --title "codemeter-v0.2.5 — <short description>" \
  --notes "..."
```

…then update `docs/codemeter-latest-merged.bin` + `docs/manifest.json`
in this repo to point web installers at the new release. A `release.sh`
helper in the source repo automates both.

The device picks the new release up on its next poll (~6 h cadence, or
immediately on next boot).
