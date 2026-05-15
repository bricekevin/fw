# Project Guide for AI Agents

This file provides comprehensive guidance to AI coding assistants working with this codebase.

---

## Project Overview

[ONE_PARAGRAPH_DESCRIPTION_OF_WHAT_THE_PROJECT_DOES]

**Tech Stack:**
- Frontend: [FRAMEWORK]
- Backend: [FRAMEWORK/LANGUAGE]
- Database: [DATABASE_TYPE]
- Infrastructure: [DOCKER/K8S/SERVERLESS]

**Quick Start:**
```bash
# Setup
[SETUP_COMMANDS]

# Start
[START_COMMANDS]

# Test
[TEST_COMMANDS]
```

---

## Documentation Structure

This project uses a structured documentation approach:

```
./
├── CLAUDE.md (this file)
├── README.md
└── docs/
    ├── 1_PROJECT_OVERVIEW.md      # Vision, goals, features
    ├── 2_ARCHITECTURE.md          # Tech stack, components, APIs
    ├── 3_DESIGN_SYSTEM.md         # UI/UX (colors, typography, components)
    ├── 4_QUALITY_ASSURANCE.md     # Testing strategy, coverage, CI/CD
    ├── HANDOFF_NOTES.md           # Session tracking & context
    └── Phase [N]/
        ├── PHASE_PRD.md           # Product requirements
        ├── PHASE_TASKS.md         # Task checklist with [ ] boxes
        └── PHASE_IMP.md           # Implementation guide
```

**When working, always reference:**
- `2_ARCHITECTURE.md` for patterns
- `3_DESIGN_SYSTEM.md` for UI standards
- `4_QUALITY_ASSURANCE.md` for quality requirements
- `HANDOFF_NOTES.md` for recent context

---

## Development Workflow

This project uses an **agentic, phase-based development approach**.

### Phase-Based Development

Work is organized into **phases**, each with:
- **PHASE_PRD.md** - Requirements & user stories
- **PHASE_TASKS.md** - Task checklist ([ ] pending, [~] in-progress, [x] done)
- **PHASE_IMP.md** - Implementation details & code examples

### Development Commands

**Nine commands for structured development:**

#### 1. `/1_start <project description>`
**Purpose:** Initialize new project from scratch
- Asks 3 questions (scope, starting point, quality)
- Creates foundation docs (CLAUDE, README, 7 docs)
- Sets up project structure

#### 2. `/2_pm <phase description>`
**Purpose:** Break project into phases
- Reads project docs for context
- Creates Phase N folder
- Generates PRD, TASKS, IMP files

#### 3. `/3_dev [phase number]`
**Purpose:** Implement phase tasks systematically
- Auto-detects latest phase
- Creates a worktree by default (see Parallel Sessions section)
- Finds next task from PHASE_TASKS.md
- Implements following project standards
- Writes & runs unit tests
- Updates PHASE_TASKS.md checkboxes
- Updates HANDOFF_NOTES.md

**Use for:** Structured phase work

#### 4. `/4_test [phase number]`
**Purpose:** Functional quality validation
- Runs unit tests
- Runs integration tests
- Runs E2E tests (Playwright MCP)
- Checks code quality (linting, types, security)
- Validates logs
- Performance checks
- Generates test report

**Use for:** After implementing tasks, before marking phase complete

#### 5. `/5_visual [phase number]`
**Purpose:** Visual / UI testing
- Drives Playwright MCP through critical pages
- Captures screenshots across desktop + mobile viewports
- Diffs against committed baselines
- Scans console for runtime errors
- For iOS / mobile-app projects: drives simulator and grabs screen captures
- Generates a visual report with screenshot links

**Use for:** Whenever UI changed; before merging frontend work; after design-system updates

#### 6. `/6_doc`
**Purpose:** Update existing documentation
- Scans all project docs
- Identifies outdated info
- Updates docs (never creates new files)
- Fixes code examples
- Verifies consistency

**Use for:** After testing passes, to keep docs current

#### 7. `/7_go <specific task>`
**Purpose:** Ad-hoc development outside phase tasks
- User directs the work
- Creates a worktree by default
- Follows same standards as /3_dev
- Updates HANDOFF_NOTES.md
- Tests required

**Use for:** Quick fixes, experiments, refactoring

#### 8. `/8_done`
**Purpose:** Session wrap-up before clearing context
- Checks task completion status
- Verifies documentation is updated
- Ensures all code is committed and pushed
- Cleans up the worktree if branch is merged
- Runs build/type checks and tests
- Generates handoff report with next step recommendation

**Use for:** Before clearing context, end of work session, switching projects

#### 9. `/9_deploy`
**Purpose:** Production deployment to a Mac prod server (Docker), fully automated via GitHub API + Cloudflare API + SSH.

- **First run:** provisions everything end-to-end:
  - Scaffolds CI / gitleaks / deploy-prod workflows + cloudflared config
  - Creates repo secrets via GitHub API (`gh secret set`)
  - Fetches a runner registration token via GitHub API
  - Creates a Cloudflare Tunnel + CNAME DNS record via Cloudflare API
  - SSHes to the prod Mac: installs Homebrew, Docker Desktop, cloudflared, jq
  - Zips the tracked working tree (`git archive`), scp's to prod, unzips, `git init` against the GitHub remote
  - Installs + starts the self-hosted runner unattended (correct labels)
  - Installs cloudflared as a launchd service with the credentials JSON
  - `docker compose up -d` for the first time
  - Verifies the public health endpoint via the tunnel
  - Only auto-login + (conditional) Full Disk Access stay manual on the prod Mac
- **Subsequent runs:** drift check → branch push → PR open/check → wait for CI → merge → watch deploy → verify version + tunnel health
  - Drift check: runner online (gh API), tunnel healthy (CF API), DNS resolves to the tunnel (CF API + dig), required secrets present (gh API). Auto-repairs DNS, attempts runner/tunnel restart via SSH where possible.
- Detects mode by presence of `.github/workflows/deploy-prod.yml` + `.starterpack/deploy-state.json`

**Required access (preflight bails fast if missing):**
- `gh` CLI authenticated with `repo`, `workflow`, `admin:repo_hook` scopes
- `CLOUDFLARE_API_TOKEN` env var with `Account:Cloudflare Tunnel:Edit` + `Zone:DNS:Edit` scopes
- Passwordless SSH to the prod Mac (key-based, alias in `~/.ssh/config` or `user@host`)
- `jq`, `unzip`, `cloudflared` installed on dev mac (`brew install jq unzip cloudflared`)

**Use for:** First-time prod setup (one shot, ~15 min including manual UI steps), or every release thereafter (~7 min merge → live).

---

## Typical Workflow

```
Project Setup:
/1_start <description>
    ↓
Phase Planning:
/2_pm <feature>
    ↓
Implementation (in a worktree):
/3_dev
/3_dev
/3_dev (repeat until phase tasks done)
    ↓
Functional Validation:
/4_test
    ↓
Visual Validation (if UI work):
/5_visual
    ↓
Documentation:
/6_doc
    ↓
Next Phase:
/2_pm <next feature>
(repeat)

Ad-hoc Work:
/7_go <specific task>
(anytime, for quick fixes/experiments)

Session Wrap-up:
/8_done
(before clearing context)

Release to Production:
/9_deploy
(first run: full setup; subsequent: merge to main and watch)
```

---

## Parallel Sessions & Worktrees

**Directive:** Code-modifying commands (`/3_dev`, `/7_go`, `/9_deploy` first-time setup) run in a git worktree by default. Don't ask — just create one. This lets multiple Claude Code sessions work on the same repo (even the same branch) without stepping on each other's files.

**Why:** A second session on the same checkout overwrites the first's uncommitted edits, fights over package-manager lock files, and produces confusing test failures. Worktrees give each session its own working directory backed by the shared `.git` store.

**Convention — sibling directory:**

```
parent-dir/
├── <repo>/                         # main checkout
├── <repo>-feat-auth/               # worktree for feat-auth branch
└── <repo>-bugfix-pagination/       # worktree for bugfix-pagination
```

**Default flow at the start of a code-modifying session:**

```bash
REPO_NAME=$(basename "$(git rev-parse --show-toplevel)")
BRANCH="<descriptive-name>"          # e.g., phase-3-checkout, bugfix-pagination
WORKTREE_PATH="../${REPO_NAME}-${BRANCH}"

if [ ! -d "$WORKTREE_PATH" ]; then
  git fetch origin
  if git show-ref --verify --quiet "refs/heads/$BRANCH"; then
    git worktree add "$WORKTREE_PATH" "$BRANCH"
  else
    git worktree add -b "$BRANCH" "$WORKTREE_PATH" origin/main
  fi
fi
cd "$WORKTREE_PATH"
```

**Cleanup (handled by `/8_done` when branch is merged):**
```bash
git worktree remove "$WORKTREE_PATH"
git branch -d "$BRANCH" 2>/dev/null || true
git worktree prune
```

**Gotchas:**
- Each worktree has its own `node_modules/`, `.venv/`, `dist/`. Run installers once per worktree.
- The same branch can't be checked out in two worktrees — one branch per worktree.
- `.env*` files aren't auto-copied — copy or symlink into the new worktree.

**Skip the worktree dance for:** read-only sessions, planning-only `/2_pm` runs, pure doc edits in `/6_doc`, and the very first `/1_start` on an empty project.

---

## Architecture

**System Flow:**
[COMPONENT_1] → [COMPONENT_2] → [COMPONENT_3]

**Key Components:**
- **[COMPONENT_1]**: [PURPOSE]
- **[COMPONENT_2]**: [PURPOSE]
- **[COMPONENT_3]**: [PURPOSE]

**Data Flow:**
[DESCRIBE_HOW_DATA_MOVES_THROUGH_SYSTEM]

**Key Patterns:**
- [PATTERN_1]: [BRIEF_EXPLANATION]
- [PATTERN_2]: [BRIEF_EXPLANATION]

**For details:** See `docs/2_ARCHITECTURE.md`

---

## Design System

**Colors:** [PRIMARY], [SECONDARY], [ACCENT]  
**Typography:** [FONT_1] (headings), [FONT_2] (body)  
**Spacing:** [SPACING_SYSTEM]

**When implementing UI:**
- Use defined colors (not arbitrary hex values)
- Use typography scale (not random font sizes)
- Use spacing system (not magic numbers)

**For details:** See `docs/3_DESIGN_SYSTEM.md`

---

## Quality Standards

**Test Coverage:** [X%] minimum  
**Performance:** [METRIC_TARGET]  
**Security:** [REQUIREMENTS]

**Testing Requirements:**
- Unit tests for all new code
- Integration tests for APIs/services
- E2E tests for critical flows
- All tests must pass before merging

**For details:** See `docs/4_QUALITY_ASSURANCE.md`

---

## MCP Servers Available

This project has access to Model Context Protocol (MCP) servers for enhanced capabilities.

### Sequential Thinking MCP

**Tool:** `mcp_sequential-thinking_sequentialthinking`

**Purpose:** Complex problem-solving through structured reasoning

**Use for:**
- Breaking down complex problems
- Planning architecture decisions
- Analyzing trade-offs
- Multi-step solutions
- Course correction during implementation

**Example usage:**
```javascript
mcp_sequential-thinking_sequentialthinking({
  thought: "Analyzing how to implement user authentication",
  thoughtNumber: 1,
  totalThoughts: 5,
  nextThoughtNeeded: true
})
```

### Playwright MCP

**Purpose:** Browser automation and frontend testing

**Key Tools:**

#### Navigation & Interaction
```javascript
mcp_playwright_navigate({ url: "http://localhost:3000" })
mcp_playwright_click({ selector: ".login-button" })
mcp_playwright_fill({ selector: "#email", value: "test@example.com" })
mcp_playwright_hover({ selector: ".dropdown" })
```

#### Validation
```javascript
mcp_playwright_get_visible_text()
mcp_playwright_get_visible_html()
mcp_playwright_screenshot({ name: "login-page", savePng: true })
```

#### Debugging
```javascript
mcp_playwright_console_logs({ type: "error" })
mcp_playwright_evaluate({ script: "document.title" })
```

**Use for:**
- E2E testing of user flows
- Visual regression checking
- Console error detection
- UI element validation
- Automated testing during `/4_test` and `/5_visual` commands

---

## Project Configuration

### Git Commit Guidelines

**IMPORTANT**: When creating git commits:

1. Do NOT include any AI assistant references, attributions, or co-author tags
2. Do NOT add "Generated with" or similar footer lines
3. Do NOT use emojis in commit messages
4. Commits should appear as normal developer commits
5. Keep commit messages concise and focused on what changed

### Code Style

- Follow existing patterns in the codebase
- Match the project's language and conventions
- No emojis in code comments or documentation unless explicitly requested

---

## Development Guidelines

### Code Quality
- Follow patterns from `2_ARCHITECTURE.md`
- Use design system from `3_DESIGN_SYSTEM.md` (if UI work)
- Meet standards from `4_QUALITY_ASSURANCE.md`
- Write tests proportional to the change -- new features need tests, config tweaks may not
- Add type annotations where the project uses them
- Handle errors gracefully
- Add logging where it aids debugging

### Commits
```bash
git commit -m "[type](scope): concise description

- change 1
- change 2"
```
**Types:** feat, fix, refactor, test, docs, chore

**Rules:** No emojis, no AI attribution, no co-author tags. Commits should read as normal developer commits.

### Documentation
- Update existing docs (never create new MD files)
- Keep HANDOFF_NOTES.md current
- Document decisions and trade-offs
- Update code examples when changing implementation

### Testing
- Tests should pass before committing
- Write tests proportional to the change and project quality level
- Use Playwright MCP for frontend testing when applicable
- Check logs for errors after significant changes

---

## Project-Specific Commands

### Service Management
```bash
[SERVICE_START_COMMAND]
[SERVICE_STOP_COMMAND]
[SERVICE_RESTART_COMMAND]
```

### Testing
```bash
[UNIT_TEST_COMMAND]
[INTEGRATION_TEST_COMMAND]
[E2E_TEST_COMMAND]
```

### Database
```bash
[MIGRATION_COMMAND]
[SEED_COMMAND]
[RESET_COMMAND]
```

### Logs
```bash
[LOG_VIEW_COMMAND]
[LOG_FOLLOW_COMMAND]
```

---

## Configuration

**Environment Variables:**
- `[VAR_NAME]` - [PURPOSE]
- `[VAR_NAME]` - [PURPOSE]

**Ports:**
- [PORT]: [SERVICE_NAME]
- [PORT]: [SERVICE_NAME]

**For details:** See `.env.example`

---

## Troubleshooting

**Services won't start:**
- Check [SERVICE_CHECK_COMMAND]
- Verify [REQUIREMENT]
- Review logs with [LOG_COMMAND]

**Tests failing:**
- Check test output
- Verify environment setup
- Review recent changes
- Check HANDOFF_NOTES.md for known issues

**Need context:**
- Read HANDOFF_NOTES.md for recent work
- Check git log: `git log --oneline -20`
- Review Phase N PHASE_TASKS.md for progress

---

## Quick Reference

**Load context:**
```bash
cat docs/HANDOFF_NOTES.md
git status
git log --oneline -10
```

**Find similar code:**
```bash
grep -r "[PATTERN]" src/
```

**Check project standards:**
```bash
cat docs/2_ARCHITECTURE.md | grep -A 10 "Key Patterns"
cat docs/3_DESIGN_SYSTEM.md | grep -A 20 "Colors"
cat docs/4_QUALITY_ASSURANCE.md | grep -A 5 "Coverage"
```

**Current phase progress:**
```bash
LATEST=$(ls docs/Phase\ */ -d | sed 's/.*Phase //' | sed 's/\/.*//' | sort -n | tail -1)
cat "docs/Phase $LATEST/PHASE_TASKS.md"
```

---

## Notes

[PROJECT_SPECIFIC_NOTES_CONSTRAINTS_OR_CONVENTIONS]

---

**Remember:**
- Read docs before coding
- Follow project standards
- Write tests
- Update HANDOFF_NOTES.md
- Keep documentation current

