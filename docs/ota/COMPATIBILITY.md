# Can every fielded device still reach the latest firmware?

Audited 2026-08-29 by reading the OTA source **at each release tag**, not by
inferring from dates. Answer: **yes, every generation, with one dated caveat**
(the ISRG cross-sign, at the bottom).

Only three products have units in the field: **codeMeter, priceDisplay,
socialMeter**. Every sign (coaster, Dodge Ridge, and the rest) is in hand and is
recovered over USB from `https://bricekevin.github.io/fw/<product>/`.

## The two mechanisms

| | Reads | Needs from us |
|---|---|---|
| **RELEASES-API** (older) | `api.github.com/repos/bricekevin/fw/releases?per_page=20` — compiled in, unredirectable | a **published**, tag-prefixed release whose asset is named **exactly `firmware.bin`**, in a response GitHub does not serve chunked |
| **MANIFEST** (current) | `bricekevin.github.io/fw/ota/<product>.json` — ~140 B, always `Content-Length` | the manifest to resolve, and the URL it names to return 200 |

## Every released generation

| product | versions | mechanism | reaches latest today? |
|---|---|---|---|
| codeMeter | `v0.0.2`–`v0.2.0` (legacy unprefixed), `codemeter-v0.2.2`–`v0.2.8`, `v0.3.0` | RELEASES-API, asset `firmware.bin` | **yes** — `codemeter-v0.3.2` is published with that exact asset name |
| codeMeter | `codemeter-v0.3.1`, `v0.3.2` | MANIFEST | **yes** |
| priceDisplay | `v1.0.3` and earlier | RELEASES-API | **yes** — `priceDisplay-v1.0.5` published, asset `firmware.bin` |
| priceDisplay | `v1.0.4`, `v1.0.5` | MANIFEST | **yes** |
| socialMeter | `v0.1.0` | RELEASES-API, asset `firmware.bin` | **yes — but only since 2026-08-29.** Its releases only ever carried `socialmeter-firmware.bin`, so this generation's OTA **never worked**. `socialmeter-v0.1.3` now also carries `firmware.bin`. |
| socialMeter | `v0.1.1`–`v0.1.3` | MANIFEST | **yes** |

codeMeter's legacy unprefixed tags (`v0.0.2`…`v0.2.0`) are matched by a
"prefixed **or** bare `v…`" rule in that firmware, so they still select the
current `codemeter-v0.3.2`.

## Why RELEASES-API devices work

All three products' newest release is **published permanently** and carries an
asset named exactly `firmware.bin` — see `RESCUE-RUNBOOK.md`. The list is
13,382 B against a ~17,900 B chunking threshold, and chunks about 1 sample in
30; a device retries every 6 h, so that is immaterial.

**Proven on hardware.** A genuine socialMeter `v0.1.0` was flashed to the bench
M5Stack and rescued itself unattended — parsed a 9,287 B unchunked list, matched
`socialmeter-v0.1.3` via `firmware.bin`, installed, rebooted, and then polled
the **Pages manifest**. It crossed permanently to the modern scheme.

## Why MANIFEST devices work

Manifests now name a **Pages** payload rather than a release asset, so they no
longer depend on any release being published.

**Proven on hardware** for socialMeter (0.1.2 → 0.1.3, 1,275,968 B from Pages).
codeMeter and priceDisplay were **not** separately flashed, but their download
routine is byte-for-byte the same code — diffed 2026-08-29, the only difference
between socialMeter's and priceDisplay's is one comment — and both pin the same
two roots. Every live manifest and every asset it names was verified to return
200.

Pages is also **simpler** than what it replaced: one hop with `Content-Length`,
versus a 302 chain through `release-assets.githubusercontent.com` that the
firmware follows by hand.

## The one dated caveat: the ISRG cross-sign

All three products pin exactly two roots: **USERTrust ECC** (for `github.com`)
and **ISRG Root X1**. But `bricekevin.github.io` now presents:

```
*.github.io  ->  Let's Encrypt YR1  ->  ISRG Root YR   (expires 2028-09-02)
                                        ISRG Root YR is cross-signed by
                                        ISRG Root X1   (expires 2032-09-02)
```

Fielded devices reach Pages **through that cross-sign**. Verified two ways:
`openssl` validates the live site using *only* the two pinned roots as trust
anchors, and a real device downloaded 1.2 MB from Pages.

**This is fine until 2032-09-02**, but it is a real dependency, and if Let's
Encrypt stops including the cross-signed Root YR in the served chain before
then, fielded devices lose Pages — which is now the *only* path for MANIFEST
devices.

**Remediation, when convenient:** add self-signed **ISRG Root YR** to
`ota_certs.h` in all four products so new firmware does not depend on the
cross-sign. It is additive and low risk, but it is a firmware change to four
products and deserves its own validation pass — a malformed PEM makes
`setCACert` fail and breaks *all* OTA. Do not slip it in at the end of unrelated
work. **Do not remove ISRG Root X1 or USERTrust** when doing it: X1 still anchors
the cross-sign for already-fielded units, and `github.com` still needs USERTrust
for the RESCUE path and the merged-image download.

## How to re-run this audit

`api.github.com` release dates are misleading — read the source at the tag:

```bash
for t in $(git tag | grep '^codemeter-v' | sort -V); do
  src=$(git show "$t:codemeter/ota.ino" 2>/dev/null) || continue
  printf '%-22s %s\n' "$t" \
    "$(printf '%s' "$src" | grep -q 'github.io/fw/ota/' && echo MANIFEST || echo RELEASES-API)"
done
```

Watch for **renamed paths**: codeMeter's sketch was `m5clawd/ota.ino` before
`codemeter-v0.3.0`, and a naive audit silently reports "unknown" for every
release before the rename — which is exactly the set most likely to still be in
the field.
