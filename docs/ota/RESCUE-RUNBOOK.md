# Straggler rescue runbook

**Steady state is ZERO published releases.** Every product's manifest points at
a Pages payload, so no fleet needs a published release. Measured 2026-08-29 with
all six manifests live and nothing published: **5 B, 0 releases**.

A published release is now two things only: provenance, and the deliberate,
temporary tool below.

## Who this is for

Devices fielded **before 2026-08-24** have
`api.github.com/repos/bricekevin/fw/releases?per_page=20` compiled in and cannot
be redirected. They find an update only when that list is small enough that
GitHub does not serve it chunked, and only when the newest release carrying
their tag prefix has an asset named **exactly `firmware.bin`**.

## The rotation

**One product at a time. Never two.** With one release published the list is
~4,519 B; with five it was 27,234 B and chunked.

```bash
R() { gh api "repos/bricekevin/fw/releases?per_page=100" \
        --jq ".[] | select(.tag_name==\"$1\") | .id"; }

# open
gh api -X PATCH "repos/bricekevin/fw/releases/$(R priceDisplay-v1.0.5)" -F draft=false

# ... leave it for a few poll intervals (the check cadence is 6 h, so
#     24-48 h gives a device several chances ...

# close
gh api -X PATCH "repos/bricekevin/fw/releases/$(R priceDisplay-v1.0.5)" -F draft=true
```

Then move to the next product. Draft, never delete — drafting hides a release
from the anonymous API while preserving its tag and assets, and is reversible.

Verify the window while one is open:

```bash
curl -sS --http1.1 -D - -o /tmp/r.json \
  -H 'Accept-Encoding: identity;q=1,chunked;q=0.1,*;q=0' \
  "https://api.github.com/repos/bricekevin/fw/releases?per_page=20&cb=$RANDOM" \
  | grep -i 'transfer-encoding'          # must print nothing
wc -c < /tmp/r.json
```

The `cb=$RANDOM` is not decoration. **The releases API is edge-cached**, so a
just-drafted release keeps reporting the old byte count for minutes and you will
conclude the drafting did nothing.

## Rescue readiness

Every product's current release carries an asset named exactly `firmware.bin`
(verified 2026-08-29). Two needed fixing:

- **socialMeter** ships `socialmeter-firmware.bin`, but the pre-manifest code
  `strcmp`s the bare name — so its OTA **never worked** and its 0.1.0 units were
  never recoverable. `socialmeter-v0.1.3` now carries **both** names.
- **The coaster** release `skisign-v2.1.4` was published with
  `coaster-firmware.bin` by mistake and had the identical defect. Corrected.

### The trap that caused it

`gh release upload <file>#<name>` sets a **display label, not a filename.** The
asset keeps the local file's basename. To control the asset name, name the local
file:

```bash
mkdir -p rescue && cp build/app.bin rescue/firmware.bin
gh release upload <tag> rescue/firmware.bin --repo bricekevin/fw
```

This silently reproduces the exact socialMeter defect, and nothing downstream
complains — a pre-manifest device just goes on reporting "up to date".

## What to tell a customer

**Rescue is best-effort and cannot be guaranteed.** GitHub's framing is
nondeterministic: theClaw ADR 013 measured the same 15,047 B response served
both chunked and `Content-Length` seconds apart, so no release count is provably
safe.

**The web flasher is the guarantee.** Point an affected customer at
`https://bricekevin.github.io/fw/<product>/` and have them reflash over USB.

Note that **priceDisplay's pre-manifest failure is silently green** — its
no-match branch paints "up to date". Absence of complaints is not evidence that
its fielded units can update.

## When this file can be deleted

When no pre-manifest device can plausibly still be in the field. At that point
also retire the `check_ota_window` guard in all four `release.sh` scripts
(theClaw ADR 013). Until then, both stay.
