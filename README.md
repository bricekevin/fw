# fw

Public firmware release artifacts. Each project gets its own tag prefix on
this repo; each release attaches a single `firmware.bin` asset that the
matching device pulls over OTA.

The actual source lives in private per-project repos.

## Projects

| Project | Tag prefix | Source repo (private) | First release |
| --- | --- | --- | --- |
| codeMeter (M5Stack Core) | `codemeter-v*` | [bricekevin/theClaw](https://github.com/bricekevin/theClaw) | v0.0.2 — v0.2.0 are unprefixed legacy tags; `codemeter-v0.2.1` onward |

Pre-convention tags `v0.0.2`..`v0.2.0` are all codeMeter; the client also
accepts them so devices already in the field keep updating cleanly.

## OTA discovery

The device polls:

    https://api.github.com/repos/bricekevin/fw/releases?per_page=20

…walks the array, keeps tags matching its own project prefix (or the
legacy unprefixed `vX.Y.Z` pattern, for codeMeter only), and picks the
newest by parsed semver. The chosen release's `firmware.bin` asset is
streamed into the inactive OTA slot. See ADR 011 in the source repo for
the full design — TLS-pinned trust chain, app-level rollback, etc.

## Publishing a release

From a build in the project's source repo:

```sh
# Codemeter example:
gh release create codemeter-v0.2.1 firmware.bin \
  --repo bricekevin/fw --target main \
  --title "codemeter-v0.2.1 — <short description>" \
  --notes "..."
```

The release is created on `bricekevin/fw`; the `--target main` ties the
auto-created tag to this repo's main, but the artifact and tag are what
the device actually consumes.

Devices for that project pick it up on their next poll (~6 h cadence, or
immediately on next boot).
