# Starter Pack Workflow

Visual guide to how all commands, templates, documents, and sessions interconnect.

---

## 1. Master Lifecycle

The complete picture: commands, documents they produce, and how everything connects across sessions.

```mermaid
flowchart LR
    %% ═══════════════════════════════════════════
    %% LEFT-TO-RIGHT: /start → /pm → /dev → /test → /doc → /done
    %% DOCS grouped in the middle row for easy reference
    %% ═══════════════════════════════════════════

    %% ── 1. START ────────────────────────────────
    subgraph S1[" "]
        direction TB
        TMPL[("templates/")]
        START["/start"]
        TMPL -.->|"copy & fill"| START
    end

    %% ── 2. PM ───────────────────────────────────
    subgraph S2[" "]
        direction TB
        PM["/pm"]
    end

    %% ── 3. DEV ──────────────────────────────────
    subgraph S3[" "]
        direction TB
        DEV["/dev"]
        SRC[("src/")]
        DEV -->|"code + tests"| SRC
    end

    %% ── 4. TEST ─────────────────────────────────
    subgraph S4[" "]
        direction TB
        TEST["/test"]
    end

    %% ── 5. DOC ──────────────────────────────────
    subgraph S5[" "]
        direction TB
        DOC["/doc"]
    end

    %% ── 6. GO ───────────────────────────────────
    subgraph S6[" "]
        direction TB
        GO["/go"]
    end

    %% ── 7. DONE ─────────────────────────────────
    subgraph S7[" "]
        direction TB
        DONE["/done"]
        GIT[("git")]
        DONE -->|"commit & push"| GIT
    end

    %% ── COMMAND FLOW (left → right) ─────────────
    START ==>|"foundation ready"| PM
    PM ==>|"phase planned"| DEV
    DEV ==>|"tasks done"| TEST
    TEST ==>|"passes"| DOC
    DOC ==> DONE

    %% ── LOOPS ───────────────────────────────────
    DEV -->|"repeat per task"| DEV
    DONE -->|"next phase"| PM
    DONE -->|"more tasks"| DEV
    GO -.->|"ad-hoc"| DONE

    %% ═══════════════════════════════════════════
    %% DOCUMENTS — grouped together below commands
    %% ═══════════════════════════════════════════

    subgraph DOCS["PROJECT DOCUMENTS"]
        direction LR

        subgraph FD["Foundation (created by /start)"]
            direction TB
            CLAUDE["CLAUDE.md"]
            README["README.md"]
            OV["1_PROJECT_OVERVIEW"]
            ARCH["2_ARCHITECTURE"]
            DS["3_DESIGN_SYSTEM"]
            QA["4_QUALITY_ASSURANCE"]
            ADR["decisions/"]
        end

        HN["HANDOFF_NOTES.md"]

        subgraph PD["Phase N (created by /pm)"]
            direction TB
            PRD["PHASE_PRD"]
            TASKS["PHASE_TASKS"]
            IMP["PHASE_IMP"]
        end
    end

    %% ── WHO CREATES WHAT ────────────────────────
    START -->|"creates"| FD
    START -->|"initializes"| HN
    PM -->|"creates"| PD

    %% ── WHO READS WHAT ──────────────────────────
    FD -.->|"reads for context"| PM
    PD -.->|"reads PRD + IMP"| DEV
    ARCH -.->|"patterns"| DEV
    QA -.->|"standards"| DEV
    QA -.->|"quality targets"| TEST
    ARCH -.->|"patterns"| GO

    %% ── WHO WRITES WHAT ─────────────────────────
    DEV -->|"[ ]→[~]→[x]"| TASKS
    DEV -->|"session entry"| HN
    GO -->|"session entry"| HN
    DOC -->|"updates"| FD
    DOC -->|"updates"| HN
    DONE -->|"final update"| HN
    DONE -->|"verifies [x]"| TASKS

    %% ── HANDOFF READS (context bridge) ──────────
    HN -.->|"context"| PM
    HN -.->|"context"| DEV
    HN -.->|"context"| GO
    HN -.->|"reads"| DONE

    %% ── STYLES ──────────────────────────────────
    style START fill:#8b5cf6,stroke:#7c3aed,color:#fff
    style PM fill:#3b82f6,stroke:#2563eb,color:#fff
    style DEV fill:#10b981,stroke:#059669,color:#fff
    style TEST fill:#f59e0b,stroke:#d97706,color:#fff
    style DOC fill:#6366f1,stroke:#4f46e5,color:#fff
    style GO fill:#ec4899,stroke:#db2777,color:#fff
    style DONE fill:#ef4444,stroke:#dc2626,color:#fff

    style HN fill:#d97706,stroke:#b45309,color:#fff

    style DOCS fill:#0f172a,stroke:#334155,color:#94a3b8
    style FD fill:#1e1b4b,stroke:#4338ca,color:#e2e8f0
    style PD fill:#172554,stroke:#2563eb,color:#e2e8f0

    style S1 fill:transparent,stroke:none
    style S2 fill:transparent,stroke:none
    style S3 fill:transparent,stroke:none
    style S4 fill:transparent,stroke:none
    style S5 fill:transparent,stroke:none
    style S6 fill:transparent,stroke:none
    style S7 fill:transparent,stroke:none
```

---

## 2. Project Initialization (`/start`)

Two entry paths depending on whether you have an existing codebase or are starting fresh.

```mermaid
flowchart TD
    USER["User runs /start &lt;description&gt;"]
    Q1{"Q1: New or Existing?"}
    Q2["Q2: Quality Level?<br/>A) Prototype  B) MVP<br/>C) Production  D) Enterprise"]

    USER --> Q1
    USER --> Q2

    %% Path A: Existing
    Q1 -->|"Existing codebase"| EXPLORE
    subgraph EXPLORE["PATH A: Automated Exploration"]
        direction TB
        E1["Analyze directory structure"]
        E2["Identify tech stack"]
        E3["Detect architecture patterns"]
        E4["Find config & environment"]
        E5["Examine existing tests"]
        E6["Read key files"]
        E7["Map integration points"]
        E8["Present analysis for confirmation"]
        E1 --> E2 --> E3 --> E4 --> E5 --> E6 --> E7 --> E8
    end

    %% Path B: New
    Q1 -->|"New project"| DESIGN
    subgraph DESIGN["PATH B: Conversational Design"]
        direction TB
        D1["Gather requirements<br/>(single prompt)"]
        D2["Infer tech needs"]
        D3["Recommend stack"]
        D4["Design data model"]
        D5["Define API endpoints"]
        D6["Identify MVP features"]
        D7["Present design for confirmation"]
        D1 --> D2 --> D3 --> D4 --> D5 --> D6 --> D7
    end

    %% Both paths converge
    EXPLORE --> ADR
    DESIGN --> ADR

    subgraph ADR["Architectural Decisions"]
        A1["Create docs/decisions/"]
        A2["000-index.md"]
        A3["001-tech-stack.md"]
        A4["002-database.md"]
        A1 --> A2 & A3 & A4
    end

    ADR --> COPY

    subgraph COPY["Copy & Fill Templates"]
        direction TB
        C1["Copy templates/ to project root"]
        C2["Fill 7 core docs with<br/>real content from exploration"]
        C3["Validate: no placeholders remain"]
        C1 --> C2 --> C3
    end

    %% Quality level influences depth
    Q2 -.->|"gates doc depth"| COPY

    COPY --> READY["Foundation ready<br/>Next: /pm"]

    style USER fill:#8b5cf6,stroke:#7c3aed,color:#fff
    style EXPLORE fill:#e0f2fe,stroke:#0284c7
    style DESIGN fill:#fef3c7,stroke:#d97706
    style ADR fill:#f3e8ff,stroke:#9333ea
    style COPY fill:#ecfdf5,stroke:#059669
    style READY fill:#10b981,stroke:#059669,color:#fff
```

---

## 3. Template Flow

What gets copied from `templates/` and becomes the project's living documentation.

```mermaid
flowchart LR
    subgraph TEMPLATES["templates/ (starter pack source)"]
        direction TB
        T_CLAUDE["CLAUDE.md"]
        T_README["README.md"]
        T_DOCS["docs/"]
        T_1["1_PROJECT_OVERVIEW.md"]
        T_2["2_ARCHITECTURE.md"]
        T_3["3_DESIGN_SYSTEM.md"]
        T_4["4_QUALITY_ASSURANCE.md"]
        T_HN["HANDOFF_NOTES.md"]
        T_DEC["decisions/<br/>000-index.md<br/>001-tech-stack.md<br/>002-database.md<br/>TEMPLATE-adr.md"]
        T_PHASE["Phase [N]/<br/>PHASE_PRD.md<br/>PHASE_TASKS.md<br/>PHASE_IMP.md"]
    end

    subgraph PROJECT["Your Project (after /start)"]
        direction TB
        P_CLAUDE["CLAUDE.md<br/><i>filled with real stack,<br/>commands, patterns</i>"]
        P_README["README.md<br/><i>filled with setup,<br/>install, usage</i>"]
        P_1["1_PROJECT_OVERVIEW.md<br/><i>vision, goals, features</i>"]
        P_2["2_ARCHITECTURE.md<br/><i>components, APIs, data flow</i>"]
        P_3["3_DESIGN_SYSTEM.md<br/><i>colors, typography, spacing</i>"]
        P_4["4_QUALITY_ASSURANCE.md<br/><i>test strategy, coverage</i>"]
        P_HN["HANDOFF_NOTES.md<br/><i>Session 0 entry</i>"]
        P_DEC["decisions/<br/><i>real ADRs from /start</i>"]
    end

    subgraph PHASES["Created Later (by /pm)"]
        direction TB
        PH["Phase N/<br/>PHASE_PRD.md<br/>PHASE_TASKS.md<br/>PHASE_IMP.md"]
    end

    T_CLAUDE -->|"/start copies & fills"| P_CLAUDE
    T_README -->|"/start copies & fills"| P_README
    T_1 -->|"/start copies & fills"| P_1
    T_2 -->|"/start copies & fills"| P_2
    T_3 -->|"/start copies & fills"| P_3
    T_4 -->|"/start copies & fills"| P_4
    T_HN -->|"/start copies & fills"| P_HN
    T_DEC -->|"/start copies & fills"| P_DEC
    T_PHASE -->|"/pm copies & fills"| PH

    style TEMPLATES fill:#f8fafc,stroke:#94a3b8
    style PROJECT fill:#ecfdf5,stroke:#059669
    style PHASES fill:#eff6ff,stroke:#3b82f6
```

---

## 4. Command Data Flow

Which files each command **reads** vs **writes**. This is the core interconnection map.

```mermaid
flowchart TB
    subgraph COMMANDS["Commands"]
        direction TB
        C_START["/start"]
        C_PM["/pm"]
        C_DEV["/dev"]
        C_TEST["/test"]
        C_DOC["/doc"]
        C_GO["/go"]
        C_DONE["/done"]
    end

    subgraph FOUNDATION["Foundation Docs"]
        direction TB
        CLAUDE["CLAUDE.md"]
        README["README.md"]
        OVERVIEW["1_PROJECT_OVERVIEW"]
        ARCH["2_ARCHITECTURE"]
        DESIGN_SYS["3_DESIGN_SYSTEM"]
        QA["4_QUALITY_ASSURANCE"]
    end

    subgraph SESSION["Session Tracking"]
        HANDOFF["HANDOFF_NOTES.md"]
    end

    subgraph PHASE_DOCS["Phase N Docs"]
        direction TB
        PRD["PHASE_PRD.md"]
        TASKS["PHASE_TASKS.md"]
        IMP["PHASE_IMP.md"]
    end

    subgraph DECISIONS["Decisions"]
        ADR["decisions/*.md"]
    end

    %% /start - WRITES everything
    C_START ==>|"creates all"| CLAUDE & README & OVERVIEW & ARCH & DESIGN_SYS & QA & HANDOFF & ADR

    %% /pm - reads foundation, writes phase docs
    OVERVIEW & ARCH & DESIGN_SYS & QA & HANDOFF -.->|"reads"| C_PM
    C_PM ==>|"creates"| PRD & TASKS & IMP

    %% /dev - reads phase + foundation, writes tasks + handoff
    CLAUDE & ARCH & DESIGN_SYS & QA & HANDOFF -.->|"reads"| C_DEV
    PRD & TASKS & IMP -.->|"reads"| C_DEV
    C_DEV ==>|"updates"| TASKS & HANDOFF

    %% /test - reads phase + QA standards
    QA & CLAUDE -.->|"reads"| C_TEST
    PRD & TASKS & IMP -.->|"reads"| C_TEST

    %% /doc - reads everything, updates everything
    CLAUDE & README & OVERVIEW & ARCH & DESIGN_SYS & QA & HANDOFF -.->|"reads"| C_DOC
    PRD & TASKS & IMP -.->|"reads"| C_DOC
    C_DOC ==>|"updates"| CLAUDE & README & OVERVIEW & ARCH & DESIGN_SYS & QA & HANDOFF

    %% /go - reads foundation, writes handoff
    CLAUDE & ARCH & DESIGN_SYS & QA & HANDOFF -.->|"reads"| C_GO
    C_GO ==>|"updates"| HANDOFF

    %% /done - reads tasks + handoff, writes handoff + tasks
    HANDOFF & TASKS -.->|"reads"| C_DONE
    C_DONE ==>|"updates"| HANDOFF & TASKS

    style C_START fill:#8b5cf6,stroke:#7c3aed,color:#fff
    style C_PM fill:#3b82f6,stroke:#2563eb,color:#fff
    style C_DEV fill:#10b981,stroke:#059669,color:#fff
    style C_TEST fill:#f59e0b,stroke:#d97706,color:#fff
    style C_DOC fill:#6366f1,stroke:#4f46e5,color:#fff
    style C_GO fill:#ec4899,stroke:#db2777,color:#fff
    style C_DONE fill:#ef4444,stroke:#dc2626,color:#fff
    style HANDOFF fill:#fef3c7,stroke:#d97706
```

---

## 5. Session Lifecycle

How HANDOFF_NOTES.md bridges context across sessions when the conversation is cleared.

```mermaid
sequenceDiagram
    participant U as User
    participant S1 as Session 1
    participant HN as HANDOFF_NOTES.md
    participant PT as PHASE_TASKS.md
    participant GIT as Git History
    participant S2 as Session 2

    Note over S1: /dev or /go
    S1->>S1: Implement code
    S1->>PT: Mark tasks [x]
    S1->>GIT: git commit
    S1->>HN: Write session entry<br/>(completed, files changed,<br/>issues, next steps)

    Note over S1: /done
    S1->>S1: Verify all committed
    S1->>S1: Check task statuses
    S1->>HN: Final session update
    S1->>GIT: git push

    U->>U: Clear context

    Note over S2: New session starts
    U->>S2: /dev or /pm or /go

    S2->>HN: Read latest session entry
    S2->>PT: Read task statuses [ ] [~] [x]
    S2->>GIT: git log (recent commits)

    Note over S2: Full context restored<br/>from files, not memory

    S2->>S2: Continue where S1 left off
```

---

## 6. The Development Loop

The inner cycle that repeats for each phase until all tasks are done.

```mermaid
flowchart TD
    PM_DONE["Phase planned<br/>(PRD + TASKS + IMP exist)"]

    subgraph DEV_LOOP["Development Cycle (repeat per task)"]
        direction TB
        FIND["Find next task<br/>in PHASE_TASKS.md<br/>(first [ ] or [~])"]
        MARK_WIP["Mark task [~]<br/>(in progress)"]
        IMPLEMENT["Implement code<br/>following ARCH + IMP patterns"]
        WRITE_TESTS["Write tests"]
        RUN_TESTS["Run tests"]
        COMMIT["git commit"]
        MARK_DONE["Mark task [x]<br/>(complete)"]
        UPDATE_HN["Update HANDOFF_NOTES"]

        FIND --> MARK_WIP --> IMPLEMENT --> WRITE_TESTS --> RUN_TESTS
        RUN_TESTS -->|"pass"| COMMIT --> MARK_DONE --> UPDATE_HN
        RUN_TESTS -->|"fail"| IMPLEMENT
        UPDATE_HN -->|"more tasks?"| FIND
    end

    PM_DONE --> FIND

    subgraph VALIDATE["Validation"]
        TEST_CMD["/test<br/>Full quality check"]
        DOC_CMD["/doc<br/>Update docs"]
        TEST_CMD --> DOC_CMD
    end

    UPDATE_HN -->|"all tasks [x]"| TEST_CMD

    subgraph WRAP["Session Management"]
        DONE_CMD["/done<br/>Wrap up session"]
        CLEAR["Clear context"]
        NEXT["Next session"]
        DONE_CMD --> CLEAR --> NEXT
    end

    DOC_CMD --> DONE_CMD
    NEXT -->|"more phases"| PM_NEW["/pm (next phase)"]
    NEXT -->|"same phase, more tasks"| FIND
    PM_NEW --> FIND

    style PM_DONE fill:#3b82f6,stroke:#2563eb,color:#fff
    style DEV_LOOP fill:#ecfdf5,stroke:#059669
    style VALIDATE fill:#fef3c7,stroke:#d97706
    style WRAP fill:#fee2e2,stroke:#dc2626
    style PM_NEW fill:#3b82f6,stroke:#2563eb,color:#fff
```

---

## 7. File Relationship Matrix

Quick reference: what each command reads and writes.

| | CLAUDE | README | 1_OVERVIEW | 2_ARCH | 3_DESIGN | 4_QA | HANDOFF | TASKS | PRD | IMP | ADRs |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **/start** | **W** | **W** | **W** | **W** | **W** | **W** | **W** | | | | **W** |
| **/pm** | | | R | R | R | R | R | **W** | **W** | **W** | |
| **/dev** | R | | | R | R | R | R **W** | R **W** | R | R | |
| **/test** | R | | | | | R | | R | R | R | |
| **/doc** | R **W** | R **W** | R **W** | R **W** | R **W** | R **W** | R **W** | R | R | R | |
| **/go** | R | | | R | R | R | R **W** | | | | |
| **/done** | | | | | | | R **W** | R **W** | | | |

**R** = reads | **W** = writes | **R W** = reads and writes

---

## 8. Quality Level Scaling

The Q2 answer from `/start` influences how much ceremony each command applies.

```mermaid
flowchart LR
    subgraph PROTO["A) Prototype"]
        P1["CLAUDE.md"]
        P2["README.md"]
        P3["2_ARCHITECTURE.md<br/>(light)"]
        P4["HANDOFF_NOTES.md"]
    end

    subgraph MVP["B) MVP"]
        M1["All core docs"]
        M2["1-2 ADRs"]
        M3["Concise content"]
    end

    subgraph PROD["C) Production"]
        PR1["Full doc set"]
        PR2["Thorough ADRs"]
        PR3["Detailed standards"]
    end

    subgraph ENT["D) Enterprise"]
        E1["Comprehensive docs"]
        E2["All ADRs"]
        E3["Security considerations"]
        E4["Detailed quality gates"]
    end

    PROTO --- MVP --- PROD --- ENT

    style PROTO fill:#fef3c7,stroke:#d97706
    style MVP fill:#e0f2fe,stroke:#0284c7
    style PROD fill:#ecfdf5,stroke:#059669
    style ENT fill:#f3e8ff,stroke:#9333ea
```

**How this affects each command:**

| Command | Prototype | MVP | Production | Enterprise |
|---------|-----------|-----|------------|------------|
| /start | 3-4 docs, skip design system & detailed ADRs | Standard doc set, concise | Full docs, thorough ADRs | Deep analysis, security, compliance |
| /pm | TASKS only for simple features | All 3 docs, concise | All 3 + dependency graph | Detailed PRD, risk analysis |
| /dev | Implement + quick verify | Implement + unit tests | Full tests + patterns | Tests + security + perf |
| /test | Quick smoke test | Unit + integration | Full suite + E2E | Full suite + security + perf + logs |
| /doc | Update what changed | Check related docs | Cross-doc consistency | Full sweep |
| /done | Git commit + handoff note | + task status + sync | + build check | + full validation |

---

## 9. Ad-Hoc Work (`/go`)

`/go` operates outside the phase structure but follows the same standards.

```mermaid
flowchart LR
    subgraph STRUCTURED["Structured Path"]
        direction TB
        S_PM["/pm"] --> S_DEV["/dev"] --> S_TEST["/test"] --> S_DOC["/doc"]
    end

    subgraph ADHOC["Ad-Hoc Path"]
        direction TB
        A_GO["/go &lt;task&gt;"]
        A_GO --> A_IMPL["Implement"]
        A_IMPL --> A_TEST["Test"]
        A_TEST --> A_HN["Update HANDOFF_NOTES"]
    end

    DONE_S["/done"]

    STRUCTURED --> DONE_S
    ADHOC --> DONE_S

    DONE_S --> NEXT_SESSION["Next Session"]

    style STRUCTURED fill:#ecfdf5,stroke:#059669
    style ADHOC fill:#fdf2f8,stroke:#db2777
    style DONE_S fill:#ef4444,stroke:#dc2626,color:#fff
```

**Key difference:** `/go` is user-directed and not tied to PHASE_TASKS.md. Use it for bug fixes, experiments, refactoring, or anything outside the current phase plan. Same quality standards apply.

---

## 10. Complete File Tree (After Full Setup)

```
project/
├── CLAUDE.md                              ← AI agent guide (filled by /start)
├── README.md                              ← Project readme (filled by /start)
│
├── .claude/
│   └── commands/                          ← Slash commands (from starter pack)
│       ├── 1_start.md                        /start
│       ├── 2_pm.md                           /pm
│       ├── 3_dev.md                          /dev
│       ├── 4_test.md                         /test
│       ├── 5_doc.md                          /doc
│       ├── 6_go.md                           /go
│       └── 7_done.md                         /done
│
├── docs/
│   ├── 1_PROJECT_OVERVIEW.md              ← Vision, goals, features
│   ├── 2_ARCHITECTURE.md                  ← Tech stack, components, APIs
│   ├── 3_DESIGN_SYSTEM.md                 ← Colors, typography, spacing
│   ├── 4_QUALITY_ASSURANCE.md             ← Test strategy, coverage targets
│   ├── HANDOFF_NOTES.md                   ← Session-by-session tracking
│   │
│   ├── decisions/                         ← Architectural Decision Records
│   │   ├── 000-index.md
│   │   ├── 001-tech-stack-selection.md
│   │   ├── 002-database-choice.md
│   │   └── ...
│   │
│   ├── Phase 1/                           ← Created by /pm
│   │   ├── PHASE_PRD.md                      Requirements & user stories
│   │   ├── PHASE_TASKS.md                    Checkbox task list
│   │   ├── PHASE_IMP.md                      Implementation guide
│   │   └── DEPENDENCIES.md                   Phase dependency graph
│   │
│   ├── Phase 2/                           ← Created by next /pm
│   │   └── ...
│   └── ...
│
├── src/                                   ← Your actual code
│   └── ...                                   (built by /dev and /go)
│
└── templates/                             ← Starter pack originals
    ├── CLAUDE.md                             (copied to root by /start)
    ├── README.md
    └── docs/
        └── ...
```

---

## Quick Reference

```
/start ─── Creates foundation ──────────────────────────── One time
/pm ────── Plans a phase ───────────────────────────────── Per feature
/dev ───── Implements tasks ─── ↺ repeat ───────────────── Per task
/test ──── Validates quality ───────────────────────────── Per phase
/doc ───── Updates docs ────────────────────────────────── After /test
/go ────── Ad-hoc work ─────────────────────────────────── Anytime
/done ──── Wraps up session ────────────────────────────── Before context clear
```
