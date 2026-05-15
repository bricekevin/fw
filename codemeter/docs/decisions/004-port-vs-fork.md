# ADR 004: Reference Projects Stay Read-Only, Do Not Fork

**Date:** 2026-05-14 (revised 2026-05-15)
**Status:** Accepted
**Deciders:** Kevin Brice
**Related:** [001-tech-stack-selection](001-tech-stack-selection.md), [006-start-from-crypto-ticker](006-start-from-crypto-ticker.md)

---

## Revision note (2026-05-15)

Originally this ADR only concerned [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter). On 2026-05-15 a **second** reference project entered the picture: the user's working `Crypto_Coin_TickerUS_Stock`, which M5Clawd is copy-and-stripped from (see [ADR 006](006-start-from-crypto-ticker.md)). This ADR is revised so its "reference projects are read-only, never forked, never edited" rule covers **both** reference projects.

---

## Context

M5Clawd draws on two existing codebases:

1. **`Clawdmeter/`** ([HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter)) — the *concept* origin. Different hardware (Waveshare ESP32-S3 AMOLED), different transport (BLE + host daemon). Encumbered by a "licensing gray area" disclaimer about Anthropic's proprietary brand fonts and the copyrighted Clawd mascot — see its `README.md`.
2. **`Crypto_Coin_TickerUS_Stock`** (`~/GIT/Crypto_Coin_TickerUS_Stock`) — the *scaffolding* source. A working M5Stack Core ticker the user runs. M5Clawd is copy-and-stripped from it ([ADR 006](006-start-from-crypto-ticker.md)). It is also the user's **live device firmware** — disturbing it could break a device they actually use.

For each reference, the structural options are: fork it, fork-and-gut it, or treat it as a read-only reference and copy what we need into our own tree.

---

## Decision

**Neither reference project is forked or edited. Both stay read-only. `m5clawd/` is our own tree.**

- **`m5clawd/`** is created as a copy-and-strip of the crypto ticker's canonical sketch ([ADR 006](006-start-from-crypto-ticker.md)) — copied *out*, not branched *from*. It does not carry the crypto ticker's git history.
- **`Clawdmeter/`** is a plain `git clone` kept as a read-only concept reference.
- **`Crypto_Coin_TickerUS_Stock`** lives at its own path (`~/GIT/`), entirely outside this project tree, and is never modified by M5Clawd work.

Layout:

```text
~/GIT/theClaw/                  # this project
├── CLAUDE.md                   # M5Clawd project guide
├── README.md                   # M5Clawd quickstart
├── docs/                       # M5Clawd docs (this dir)
├── Clawdmeter/                 # read-only concept reference (git clone of upstream)
└── m5clawd/                    # OUR firmware — copy-and-stripped from the crypto ticker
    ├── m5clawd.ino             # main sketch (+ .ino tabs)
    ├── WiFiManager.cpp/.h      # vendored, copied from the crypto ticker
    ├── wm_*.h, strings_en.h    # vendored WiFiManager support files
    ├── data/                   # SPIFFS PNG assets (M5Clawd's own art)
    ├── setup-toolchain.sh
    └── flash.sh

~/GIT/Crypto_Coin_TickerUS_Stock/   # SEPARATE — read-only scaffolding source, never edited
```

**Rules:**

- **`Clawdmeter/` is read-only.** Reference only — for the Anthropic poll trick and rate-limit header names.
- **`Crypto_Coin_TickerUS_Stock` is read-only.** We copy *from* it once (the copy-strip) and never write *to* it. It is the user's working device firmware.
- **No `git submodule`, no fork** for either. Plain clones / copies; manual re-sync if ever needed.
- **Vendored code we copy in** (the tzapu WiFiManager files from the crypto ticker) lives directly in `m5clawd/` with its MIT license intact.

---

## Alternatives Considered

### Option 1: Fork a reference project and evolve M5Clawd inside it

**Description:** Fork either `Clawdmeter` or the crypto ticker, branch it, evolve M5Clawd in that repo and history.

**Pros:**
- One repo, git history preserved.
- For the crypto ticker: no copy step.

**Cons:**
- **Clawdmeter:** carries daemon code, AMOLED firmware, LVGL — all dead weight; the codebases share concepts, not code.
- **Crypto ticker:** carries ~39 stale `.ino` files, 29 shell scripts, a flash tool, binaries — a huge cleanup tax; and it is the user's *live device firmware*, risky to evolve in place.
- Either fork's history misleadingly tells someone else's project story.

**Rejected because:** Both forks import far more cruft and risk than the history-preservation is worth.

### Option 2: Fork and gut

**Description:** Fork a reference, delete everything irrelevant, rebuild.

**Pros:**
- Inherits a license file / contributor list.

**Cons:**
- Misleading history (the early commits aren't M5Clawd's).
- Inherits licensing baggage (Clawdmeter's font/mascot gray area; the crypto ticker's absent license).

**Rejected because:** No real benefit; the misleading history is a persistent small cost.

### Option 3: Vendor a reference as a git submodule

**Description:** Add a reference as `git submodule add`.

**Pros:**
- Pinned commit, `git submodule update` semantics.

**Cons:**
- Submodule maintenance tax (`--recurse-submodules` for everyone).
- We need references for *reading* and a *one-time copy*, not as build-time inputs.

**Rejected because:** Submodule ceremony exceeds the value.

---

## Consequences

### Positive

- **Clean tree.** `m5clawd/` carries only what M5Clawd needs — none of the crypto ticker's stale sketches / scripts, none of Clawdmeter's daemon.
- **Independent license footing.** We pick a license for M5Clawd's own code; Clawdmeter's font/mascot gray area stays contained to `Clawdmeter/`.
- **Unambiguous boundaries.** "Ours" (`m5clawd/`) vs "concept reference" (`Clawdmeter/`) vs "scaffolding source" (the separate crypto ticker repo) are three clearly distinct locations.
- **No accidental edits to a reference.** Read-only-by-convention. The crypto ticker — the user's live device firmware — is structurally safe because it isn't even inside this tree.

### Negative

- **Manual reference syncs.** If the crypto ticker's WiFiManager gets a bug fix later, M5Clawd won't get it automatically. Mitigation: low-churn code; note the copy date in `HANDOFF_NOTES.md`.
- **No PR path back.** Improvements we make to the copied WiFiManager glue don't flow back to the crypto ticker. Mitigation: if an improvement is genuinely portable, copy it back by hand.

### Neutral

- **`theClaw` (the parent dir) is the de-facto repo root** for this work. `HANDOFF_NOTES.md` flags `git init` as a follow-up.
- **`Clawdmeter/`'s `screenshots/`, `assets/`, `tools/`** stay browsable as references without being part of our build.

---

## Implementation

**Timeline:** Immediate for `Clawdmeter/` (already cloned); the `m5clawd/` copy-strip is Phase 1 task 1.

**Steps:**

1. Done: `git clone https://github.com/HermannBjorgvin/Clawdmeter.git Clawdmeter/`
2. Phase 1: copy-and-strip the crypto ticker into `m5clawd/` per [ADR 006](006-start-from-crypto-ticker.md)
3. `HANDOFF_NOTES.md` records the Clawdmeter clone SHA and the crypto-ticker copy date

**Dependencies:** Read access to `~/GIT/Crypto_Coin_TickerUS_Stock`

**Risks:**

- **Editing a reference by mistake.** An agent could `cd` into the crypto ticker and edit it. Mitigation: `CLAUDE.md` states explicitly that both references are read-only; the crypto ticker is outside the project tree, lowering the odds.

---

## Validation

**How will we know this was the right call?**

- 3 months in, neither reference project has been edited.
- We can describe `m5clawd/` without constantly contrasting it against a reference — the boundary is clean.
- A contributor working in `m5clawd/` doesn't need either reference repo to build and flash.

**Review Date:** End of Phase 2

---

## References

- [ADR 006: Start from a copy-and-strip of the crypto ticker](006-start-from-crypto-ticker.md)
- [Upstream Clawdmeter README](../../Clawdmeter/README.md)
- [Upstream Clawdmeter licensing warning](../../Clawdmeter/README.md#licensing-gray-area-warning)
- Scaffolding source: `~/GIT/Crypto_Coin_TickerUS_Stock`

---

**Last Updated:** 2026-05-15
**Updated By:** Kevin Brice
