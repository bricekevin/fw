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

## Proven, not assumed (2026-08-29)

A genuine pre-manifest **socialMeter 0.1.0** — the releases-API scanner, the
version whose OTA had never worked — was flashed to the bench M5Stack and
rescued itself unattended:

```
[ota] checking https://api.github.com/repos/bricekevin/fw/releases?per_page=20
[ota] body 9287 bytes (heap free 202528)
[ota] parsed 2 releases
[ota] AVAILABLE: socialmeter-v0.1.3 (running 0.1.0) -> auto-installing
[ota] downloading .../socialmeter-v0.1.3/firmware.bin
[ota] 302 -> release-assets.githubusercontent.com/...
[ota] download started, 1275968 bytes
[ota] installed -> reboot (pending verify)
...
Social Meter v0.1.3 booting
[ota] checking https://bricekevin.github.io/fw/ota/socialmeter.json
[ota] up to date (latest socialmeter-v0.1.3, running 0.1.3)
[ota] new image committed (poll succeeded)
```

The last three lines are the point: the device **crossed from pre-manifest to
manifest** and is now permanently immune to the whole problem. It found the
release only because of the `firmware.bin` asset added that day — before it,
`strcmp` failed and the device reported "up to date" forever.

**A device rescues itself on its very next poll.** The wait below is device
poll cadence, not process: nothing happens faster by watching it.

## The rotation

**Up to three products at a time.** Measured with all three M5Stack releases
published: **13,382 B, chunked 1 of 30 samples (~3%).**

Measure the *rate*, not a single verdict. One clean sample proves nothing here,
and one chunked sample is not a blocker either — what matters to a fielded
device is per-poll failure probability, because it simply retries in 6 h. At ~3%
a device is essentially certain to get through within a day (0.03^4 ≈ 1 in a
million after four polls). `chunkrate.sh` in the session scratchpad does this;
the one-liner is 25 requests counting `Transfer-Encoding: chunked`.

Contrast the *publish* guard in `release.sh`, which refuses on any chunked
sample at all. That is deliberately stricter and should stay: pushing a release
into a bad window strands devices with no retry that helps, whereas a rescue
window is open for days and every poll is another chance.

Do NOT publish all five. That was 27,234 B and came back chunked. And keep an
eye on the number rather than the count — ADR 013 observed the same **15,047 B**
response served both chunked and `Content-Length` seconds apart, so treat
anything near 15 KB as the danger zone regardless of how many releases it is.
Dropping a redundant duplicate asset from socialMeter took the current batch
from 15,284 B back to 13,382 B; assets cost real headroom.

```bash
R() { gh api "repos/bricekevin/fw/releases?per_page=100" \
        --jq ".[] | select(.tag_name==\"$1\") | .id"; }

# open / close
gh api -X PATCH "repos/bricekevin/fw/releases/$(R <tag>)" -F draft=false
gh api -X PATCH "repos/bricekevin/fw/releases/$(R <tag>)" -F draft=true
```

Leave a batch open long enough for a device to poll: the cadence is **6 h**, so
24-48 h gives a fielded unit four to eight chances. Then draft that batch and
open the next. Draft, never delete — drafting hides a release from the anonymous
API while preserving its tag and assets, and is reversible.

**Batch 1, opened 2026-08-29:** `codemeter-v0.3.2`, `priceDisplay-v1.0.5`,
`socialmeter-v0.1.3` (the three largest fleets; priceDisplay is also the one
that fails silently green, so it is the most likely to hide stragglers).
**Batch 2, still to run:** `skisign-v2.1.4`, `skisign-dr-v2.1.3`.

Check what an anonymous device actually sees — `gh api` is authenticated and
will show you drafts, which is misleading:

```bash
curl -s "https://api.github.com/repos/bricekevin/fw/releases?per_page=20&cb=$RANDOM" \
  | python3 -c "
import json,sys
for r in json.load(sys.stdin):
    n=[a['''name'''] for a in r['''assets''']]
    print(f\"  {r['''tag_name''']:<22} {'''OK''' if '''firmware.bin''' in n else '''NOT RESCUABLE'''} {n}\")
"
```

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
