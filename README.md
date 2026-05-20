# fw

Public firmware release artifacts for codeMeter (and any future devices in
this family). This repo exists only to host versioned `firmware.bin`
release assets for device-side OTA — it is intentionally empty of source.

The source, docs, and history live at
[`bricekevin/theClaw`](https://github.com/bricekevin/theClaw) (private).

## OTA endpoint

Device firmware polls:

    https://api.github.com/repos/bricekevin/fw/releases/latest

…and downloads the `firmware.bin` asset from the returned release. See ADR
011 in the source repo for the design, including the TLS-pinned trust
chain (USERTrust ECC for api.github.com, ISRG Root X1 for
release-assets.githubusercontent.com) and the app-level rollback safety
net on a crash.

## Publishing a new release

From a build in the source repo:

```sh
gh release create vX.Y.Z firmware.bin \
  --repo bricekevin/fw --target main \
  --title "vX.Y.Z — <short description>" \
  --notes "..."
```

The device picks it up automatically on its next 6 h poll (or instantly on
next boot).
