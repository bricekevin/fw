# The releases list: who reads it, and what to keep in it

**Nothing needs the releases list to update any more.** Every product's manifest
names a Pages payload (theClaw `docs/decisions/014-ota-assets-on-pages.md`), so
the current fleet never touches this API.

The list has exactly **one** audience left: devices fielded before 2026-08-24,
which have
`api.github.com/repos/bricekevin/fw/releases?per_page=20` compiled in and cannot
be redirected. They find an update only when

- the list is small enough that GitHub does not serve it **chunked**
  (Arduino-ESP32 1.0.4 cannot parse chunked — it reports success with zero
  releases), and
- the newest release carrying their tag prefix has an asset named **exactly
  `firmware.bin`**.

## The steady state — this is not a temporary window

| product | in the field? | release | why |
|---|---|---|---|
| codeMeter | **yes** | `codemeter-v0.3.2` **published** | permanent rescue window |
| priceDisplay | **yes** | `priceDisplay-v1.0.5` **published** | permanent rescue window |
| socialMeter | **yes** | `socialmeter-v0.1.3` **published** | permanent rescue window |
| coaster (`skisign`) | no — all in hand | drafted | a published sign release rescues nobody |
| Dodge Ridge (`skisign-dr`) | no — all in hand | drafted | ” |

Only the three PROD products have units the owner cannot physically reach. Every
sign — coaster, Dodge Ridge, and the rest — is in hand and can be USB-flashed
from `https://bricekevin.github.io/fw/<product>/`, so publishing a sign release
buys nothing and spends bytes that the three fleets above actually need.

**Leave the three published. Indefinitely.** There is no rotation and nothing to
close. A straggler that finds its release installs manifest firmware and is
permanently fixed, so the window should stay open for as long as one might still
wake up.

This is enforced in code, in opposite directions:

- The three PROD `release.sh` scripts draft *superseded* releases and keep the
  newest published.
- skiLiftSign's `release.sh` drafts **its own** release too, so signs take
  themselves out of the list.

## The budget

| published | size | chunked |
|---|---|---|
| 0 | 5 B | 0 of 5 |
| 1 | 4,519 B | 0 of 5 |
| 3 (the steady state) | **13,382 B** | **1 of 30 (~3%)** |
| 5 | 27,234 B | chunked — do not |

Threshold is ~17,900 B, but treat **anything near 15 KB as the danger zone**:
ADR 013 measured the same 15,047 B response served both chunked and
`Content-Length` seconds apart. Framing is not a clean function of size.

~3% per-poll failure is irrelevant to a device that retries every 6 h (~1 in a
million still stuck after four polls). Measure the *rate*, not one sample:

```bash
for i in $(seq 1 25); do
  curl -sS --http1.1 -D - -o /dev/null \
    -H 'Accept-Encoding: identity;q=1,chunked;q=0.1,*;q=0' \
    "https://api.github.com/repos/bricekevin/fw/releases?per_page=20&cb=$i$RANDOM" \
  | grep -qi 'transfer-encoding: chunked' && printf C || printf .
done; echo
```

**Before adding a sixth product, check this number**, not the product count.
A new product ships manifest-native and its release can be drafted immediately,
so it need not cost anything — but only if someone remembers to draft it.

## Verify what a device actually sees

`gh api` is authenticated and **shows drafts**, which is misleading. Use an
anonymous request:

```bash
curl -s "https://api.github.com/repos/bricekevin/fw/releases?per_page=20&cb=$RANDOM" \
  | python3 -c "
import json,sys
for r in json.load(sys.stdin):
    n=[a['name'] for a in r['assets']]
    print(f\"  {r['tag_name']:<22} {'OK' if 'firmware.bin' in n else 'NOT RESCUABLE'} {n}\")
"
```

## Proven, not assumed (2026-08-29)

A genuine pre-manifest **socialMeter 0.1.0** — the releases-API scanner, the
version whose OTA had never worked — was flashed to the bench M5Stack and
rescued itself unattended:

```
[ota] checking https://api.github.com/repos/bricekevin/fw/releases?per_page=20
[ota] body 9287 bytes (heap free 202528)
[ota] parsed 2 releases
[ota] AVAILABLE: socialmeter-v0.1.3 (running 0.1.0) -> auto-installing
[ota] download started, 1275968 bytes
[ota] installed -> reboot (pending verify)
...
Social Meter v0.1.3 booting
[ota] checking https://bricekevin.github.io/fw/ota/socialmeter.json
[ota] up to date (latest socialmeter-v0.1.3, running 0.1.3)
[ota] new image committed (poll succeeded)
```

Those last lines are the point: the device **crossed from pre-manifest to
manifest** and is now permanently immune. It matched only because of the
`firmware.bin` asset added that day — before it, `strcmp` failed and the device
reported "up to date" forever.

## The asset-name trap

`gh release upload <file>#<name>` sets a **display label, not a filename.** The
asset keeps the local file's basename. This silently reproduced the socialMeter
defect on the coaster's rename release on 2026-08-29. Name the local file:

```bash
mkdir -p rescue && cp build/app.bin rescue/firmware.bin
gh release upload <tag> rescue/firmware.bin --repo bricekevin/fw
```

Nothing downstream complains when this is wrong — a pre-manifest device just
goes on reporting "up to date".

## What to tell a customer

**Rescue is best-effort and cannot be guaranteed**, because framing is
nondeterministic. **The web flasher is the guarantee** —
`https://bricekevin.github.io/fw/<product>/`, reflash over USB.

priceDisplay's pre-manifest failure is **silently green**: its no-match branch
paints "up to date". Absence of complaints is not evidence its fielded units can
update.

## When this file can be deleted

When no pre-manifest device can plausibly still be in the field. Then draft the
last three releases (list → 5 B) and retire the `check_ota_window` guard in all
four `release.sh` scripts (ADR 013). Until then, both stay.
