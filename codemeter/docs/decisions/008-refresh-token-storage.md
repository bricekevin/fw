# ADR 008: Refresh-Token Storage — Plaintext NVS, Encryption Deferred

**Date:** 2026-05-15
**Status:** Accepted
**Deciders:** Kevin Brice
**Related:** [005-secrets-storage-nvs](005-secrets-storage-nvs.md) (revises it), [007-oauth-onboarding-mechanism](007-oauth-onboarding-mechanism.md)
**Revises:** ADR 005 — extends its NVS-storage decision from a single API key to the OAuth credential record, and re-examines the encryption trade-off now that the stored secret is higher-value.

---

## Context

ADR 005 decided secrets live in plaintext NVS via `Preferences.h`, with NVS
encryption *deferred*. At that time the only real secret was an Anthropic API
key — and ADR 005's compensating control was to recommend a **scoped project
key with low limits**.

Phase 3 changes what is stored. Per ADR 007 the device now holds an OAuth
**credential record**:

- `access token` — short-lived (~24 h), low blast radius on its own.
- `refresh token` — **long-lived, high-value**. It mints fresh access tokens to
  the user's Claude account (scopes `user:inference`, `user:profile`) until it
  is revoked or expires.
- `expiresAt` — a timestamp, not a secret.

The refresh token is materially more dangerous than the API key ADR 005
contemplated, and — crucially — **it cannot be scoped down**. ADR 005's headline
mitigation ("use a low-limit scoped key") simply does not exist for an OAuth
refresh token: it is the user's whole Claude Code grant. So the encryption
question must be re-decided on its own merits, not inherited from ADR 005.

This ADR answers Epic 1 Task 1.4: is NVS encryption in scope for Phase 3
(Epic 4.2), or explicitly deferred?

### Threat model

- **Asset:** the refresh token.
- **Attacker:** someone with **physical possession** of the device and a USB
  cable. With plaintext NVS they can `esptool.py read_flash` the NVS partition
  and recover the token in minutes. A remote attacker is out of scope — the
  token is never exposed over the network in plaintext (TLS) and never logged
  (redactor).
- **Realistic scenario:** the device is a desk display the user physically
  controls. The threat is "my desk gadget was stolen / I gave it away without
  wiping it." It is **not** a nation-state extracting eFuse.
- **Backstop:** the grant is **revocable**. The user can revoke the device's
  Claude Code session from their Claude account at any time, which kills the
  refresh token regardless of who holds it. Revocation — not encryption — is the
  control that actually bounds the damage.

---

## Decision

**The OAuth credential record (access token, refresh token, expiry) is stored
in plaintext NVS via `Preferences.h`. NVS / flash encryption is explicitly
DEFERRED to the Phase 4 hardening pass — it is NOT in Phase 3 scope.** Epic 4.2
therefore becomes a *documentation* task (record the deferral and the recovery
guidance), not an implementation task.

### NVS layout (revises ADR 005)

In the existing `Preferences` namespace `"m5clawd"` (NVS keys ≤ 15 chars):

| Key         | Type     | Contents                                      |
| ----------- | -------- | --------------------------------------------- |
| `oauth_at`  | String   | access token                                  |
| `oauth_rt`  | String   | refresh token                                 |
| `oauth_exp` | uint32   | access-token expiry, **epoch seconds**        |

The ADR 005 single-string key `anthropic_key` is **retired**. A device still
holding only `anthropic_key` and no `oauth_rt` cannot refresh and boots straight
into the "re-onboard required" state (migration handled in Epic 2.1).

### Compensating controls (these are mandatory, not optional)

Because encryption is deferred and the secret cannot be scoped down, the
following must ship in Phase 3:

1. **Make revocation the headline recovery story.** The README onboarding
   section and the "re-onboard required" UI must tell the user, in plain terms:
   *if you lose the device or give it away, revoke its Claude Code session in
   your Claude account settings* — and link/where. This is the real mitigation.
2. **Reset gesture wipes the credential.** The ADR 003 button-C 5 s reset must
   clear `oauth_at` / `oauth_rt` / `oauth_exp` (verified — Epic 3/4). Wipe the
   device before handing it on.
3. **Never logged.** The `secret_redactor()` must cover *both* token types;
   neither token ever reaches the serial log (extends the ADR 005 rule).

---

## Alternatives Considered

### Option 1: Plaintext NVS + revocation (chosen)

**Pros:** Zero added setup friction — `./flash.sh` from a clean clone still
works for anyone, the project's core constraint. Consistent with ADR 005's
storage mechanism. The revocation backstop genuinely bounds the damage.

**Cons:** A thief with the device + a cable can extract the refresh token. The
window of exposure is "until the user revokes." Accepted for a single
physically-controlled dev device.

### Option 2: NVS encryption (flash encryption + encrypted NVS partition)

**Description:** Espressif's flash encryption with the NVS key in eFuse.

**Pros:** Strong — extraction becomes a hardware-grade attack.

**Cons:** Encryption keys must be provisioned at flash time and **eFuse burning
is irreversible**. It breaks the "anyone can `./flash.sh` from a clean clone"
first-time-setup flow that is the project's central constraint — exactly the
reason ADR 005 deferred it. Brick risk if mis-provisioned.

**Deferred** (not rejected) — Phase 4 hardening. See "If M5Clawd becomes a
product" below.

### Option 3: Application-layer encryption with a device-derived key

**Description:** Encrypt the credential blob with a key derived from the eFuse
MAC / chip ID before writing NVS.

**Pros:** No flash-time provisioning; some obfuscation.

**Cons:** **Security theatre.** The decryption key is derivable from the same
chip the attacker already has in hand — anyone who can `read_flash` can also
read the eFuse MAC and re-derive the key. It raises the bar by roughly nothing
against the actual threat (physical extraction) while adding code and a
corruption-recovery path.

**Rejected because:** It does not meaningfully change the threat model and
invites false confidence.

### Option 4: Don't store the refresh token; re-onboard on every access-token expiry

**Rejected because:** It re-creates the exact problem Phase 3 exists to solve —
the device would need manual attention roughly daily, failing the "7-day
unattended" promise.

---

## Consequences

### Positive

- **Setup stays frictionless** — the project's core "easy setup once cloned"
  constraint is preserved; no eFuse burning, no brick risk.
- **The damage is genuinely bounded** by revocation, which works no matter who
  holds the token — provided the docs/UI make revocation obvious (control 1).
- **One storage mechanism** (`Preferences.h`) for everything; the refresh-token
  record is a small, well-defined extension of ADR 005's layout.

### Negative

- **Plaintext high-value secret on flash.** A stolen, un-wiped device leaks a
  refresh token until revoked. This is a real, acknowledged exposure — heavier
  than ADR 005's because the token cannot be scoped down. It is accepted *only*
  because the device is a single, physically-controlled dev unit and revocation
  is available.
- **The deferral is now load-bearing.** If M5Clawd's status changes (see below),
  this decision must be revisited *before* that change ships.

### Neutral

- `oauth_exp` is stored as epoch **seconds** though the source `expiresAt` is in
  **milliseconds** (PHASE_IMP Task 1.1) — Epic 2.1 converts on write.

---

## If M5Clawd becomes a product (revisit trigger)

This ADR's risk acceptance is explicitly contingent on M5Clawd being a single
dev device. **If the project ever moves toward distributing units to other
people, NVS/flash encryption (Option 2) becomes required, not optional**, and a
new ADR must supersede this one before any unit ships. The "encryption deferred"
decision here is a Phase-3 MVP call, not a permanent product stance.

---

## Implementation

**Timeline:** Phase 3 — NVS layout in Epic 2.1; compensating-control docs in
Epic 4.2; redactor in Epic 2.1.

**Steps:**

1. Epic 2.1 — add `oauth_at` / `oauth_rt` / `oauth_exp`; retire `anthropic_key`
   with the re-onboard migration; extend `secret_redactor()` to both tokens.
2. Epic 3/4 — confirm the button-C reset clears all three OAuth keys.
3. Epic 4.2 — **documentation only**: record this deferral and write the
   device-loss revocation guidance into the README and the "re-onboard
   required" UI copy.

**Dependencies:** `Preferences.h` (Arduino-ESP32 stdlib) — no new dependency.

---

## Validation

- The credential survives reboots and ordinary reflashes; a full-erase upload
  wipes it (as with ADR 005).
- The button-C reset wipes `oauth_at` / `oauth_rt` / `oauth_exp`.
- Neither token appears in any serial log, git diff, or screenshot.
- The README and the re-onboard UI both tell the user how to revoke a lost
  device's session.

**Review Date:** End of Phase 3; mandatory re-review if M5Clawd moves toward
distribution.

---

## References

- [ADR 005](005-secrets-storage-nvs.md) — the NVS-storage decision this revises
- [ADR 007](007-oauth-onboarding-mechanism.md) — how the credential is obtained
- `docs/Phase 3/PHASE_IMP.md` — Task 1.1 credential structure; Task 1.2 rotation note
- [ESP-IDF NVS encryption guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_encryption.html)

---

**Last Updated:** 2026-05-15
**Updated By:** Kevin Brice
