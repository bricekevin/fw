# Architectural Decision Records

**Project:** M5Clawd

---

## Purpose

This directory documents the key technical decisions made for **M5Clawd** — a Claude Code usage display for the **M5Stack Core Basic (Gray, ESP32)**. It takes its concept from [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) and its codebase scaffolding from a copy-and-strip of the user's working `Crypto_Coin_TickerUS_Stock` project (see ADR 006).

Each ADR captures the context, the decision, alternatives considered, and consequences.

---

## Format

Each ADR follows this structure:

- **Context:** Why the decision needed to be made
- **Decision:** What was chosen
- **Alternatives Considered:** Other options evaluated
- **Consequences:** Pros, cons, and trade-offs

---

## Decisions

| #   | Title                                       | Date       | Status   |
| --- | ------------------------------------------- | ---------- | -------- |
| 001 | Tech stack selection                        | 2026-05-14 (rev. 2026-05-15) | Accepted |
| 002 | WiFi-direct vs BLE + host daemon            | 2026-05-14 | Accepted |
| 003 | Captive-portal onboarding via WiFiManager   | 2026-05-14 | Accepted (API-key step superseded by 007) |
| 004 | Port to sibling repo, do not fork           | 2026-05-14 (rev. 2026-05-15) | Accepted |
| 005 | Secrets storage in NVS, no host keychain    | 2026-05-14 (rev. 2026-05-15) | Accepted (revised by 008) |
| 006 | Start from a copy-and-strip of the crypto ticker | 2026-05-15 | Accepted |
| 007 | OAuth onboarding via authorize-URL + paste-back code | 2026-05-15 | Accepted (steps 2-3 refined by 009) |
| 008 | Refresh-token storage — plaintext NVS, encryption deferred | 2026-05-15 | Accepted |
| 009 | Stage-2 OAuth onboarding portal on the home LAN | 2026-05-15 | Accepted |

---

## Status Definitions

- **Proposed:** Under consideration
- **Accepted:** Decision made and either implemented or actively being implemented
- **Deprecated:** No longer relevant
- **Superseded:** Replaced by another ADR (link the successor)

---

## When to Create an ADR

Create a new ADR for decisions that:

- Have long-term impact on the firmware or onboarding UX
- Are difficult or expensive to change later (e.g., wire protocol, storage format)
- Affect multiple subsystems (e.g., display + UI + WiFi)
- Involve significant trade-offs

---

**Last Updated:** 2026-05-15
