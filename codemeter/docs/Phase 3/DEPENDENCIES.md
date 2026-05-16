# Phase Dependencies

## Dependency Graph

```mermaid
graph LR
    P0[Phase 0: Foundation + scaffold] --> P1[Phase 1: Bootstrap]
    P1 --> P2[Phase 2: Anthropic poller + Usage UI]
    P2 --> P3[Phase 3: OAuth onboarding + token refresh]
    P3 --> P4[Phase 4: Polish - splash, battery, audio]

    style P3 fill:#4ade80,stroke:#22c55e,stroke-width:3px
```

**Renumbering:** the original roadmap in `1_PROJECT_OVERVIEW.md` had Phase 3 =
Polish. Phase 2 hardware testing showed OAuth onboarding + refresh is
MVP-blocking (the device dies within a day without it), whereas Polish is not.
So OAuth onboarding becomes **Phase 3** and Polish moves to **Phase 4**.

## Phase Relationships

| Phase   | Depends On | Blocks   | Status                         |
| ------- | ---------- | -------- | ------------------------------ |
| Phase 0 | None       | Phase 1  | Complete                       |
| Phase 1 | Phase 0    | Phase 2  | Complete                       |
| Phase 2 | Phase 1    | Phase 3  | Code-complete; OAuth pivot done |
| Phase 3 | Phase 2    | Phase 4  | Planning                       |
| Phase 4 | Phase 3    | None     | Not started (was "Phase 3 Polish") |

## Intra-Phase Dependencies (Phase 3 epics)

```mermaid
graph LR
    E1[Epic 1: OAuth spike + ADRs] --> E2[Epic 2: Credential store + refresh]
    E1 --> E3[Epic 3: Onboarding flow]
    E2 --> E4[Epic 4: Resilience + secrets]
    E3 --> E4
    E2 --> E5[Epic 5: Testing + docs]
    E3 --> E5
    E4 --> E5
```

- **Epic 1 (spike + ADRs)** gates everything — it resolves the OAuth client_id,
  endpoints, refresh-grant shape, the onboarding-mechanism ADR, and the
  refresh-token storage ADR. No firmware is written against OAuth internals
  until it lands.
- **Epic 2 (refresh)** is onboarding-method-independent; it can begin as soon as
  the spike's parameters (1.1) and refresh proof (1.2) are in hand.
- **Epic 3 (onboarding)** is shaped by the Epic 1.3 ADR.
- **Epics 4 and 5** depend on 2 + 3.

## Critical Path

```
Phase 2 -> Epic 1 (spike) -> Epic 2 (refresh) + Epic 3 (onboarding)
        -> Epic 4 -> Epic 5 -> Phase 4
```

The highest-risk node is **Epic 1** — if no viable standalone OAuth flow
exists, the phase and the standalone-device premise need a rethink with the
user. It is sequenced first so that surfaces before any effort is sunk.

**Estimated timeline:** Phase 3 ≈ 1.5-2 weeks at evening/weekend pace (18 tasks;
the spike epic is the most uncertain). Phase 4 (Polish) is not yet broken down —
run `/2_pm` for it when Phase 3 completes.

---

**Note:** Update this graph when planning Phase 4.
