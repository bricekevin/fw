# [PROJECT_NAME]

[ONE_PARAGRAPH_DESCRIPTION_OF_WHAT_THE_PROJECT_DOES]

---

## Quick Start

```bash
# Setup
[SETUP_COMMANDS]

# Start
[START_COMMANDS]

# Test
[TEST_COMMANDS]
```

---

## Tech Stack

- **Frontend:** [FRAMEWORK]
- **Backend:** [FRAMEWORK/LANGUAGE]
- **Database:** [DATABASE_TYPE]
- **Infrastructure:** [DOCKER/K8S/SERVERLESS]

---

## Documentation Structure

This project uses structured documentation for development:

```
./
├── CLAUDE.md
├── README.md (this file)
└── docs/
    ├── 1_PROJECT_OVERVIEW.md      # Vision, goals, features
    ├── 2_ARCHITECTURE.md          # Tech stack, components, APIs
    ├── 3_DESIGN_SYSTEM.md         # UI/UX guidelines
    ├── 4_QUALITY_ASSURANCE.md     # Testing strategy
    ├── HANDOFF_NOTES.md           # Work session tracking
    └── Phase [N]/
        ├── PHASE_PRD.md           # Requirements
        ├── PHASE_TASKS.md         # Task checklist
        └── PHASE_IMP.md           # Implementation
```

**Key docs:**
- **1_PROJECT_OVERVIEW.md** - Start here to understand the project
- **2_ARCHITECTURE.md** - Technical architecture and patterns
- **HANDOFF_NOTES.md** - Recent work and context

---

## Development Approach

This project uses a **phase-based development approach** with structured workflows.

### Phase-Based Development

Work is organized into **phases**, each containing:
- **PHASE_PRD.md** - What to build and why
- **PHASE_TASKS.md** - Checklist of tasks ([ ] pending, [x] done)
- **PHASE_IMP.md** - How to build it with code examples

### Development Commands

**Nine commands for structured development:**

| Command | Purpose | Usage |
|---------|---------|-------|
| `/1_start` | Initialize new project | `/1_start <description>` |
| `/2_pm` | Plan phase | `/2_pm <feature>` |
| `/3_dev` | Implement tasks (in a worktree by default) | `/3_dev` or `/3_dev <phase>` |
| `/4_test` | Functional / unit / integration testing | `/4_test` or `/4_test <phase>` |
| `/5_visual` | Visual / UI testing (Playwright, simulator) | `/5_visual` or `/5_visual <phase>` |
| `/6_doc` | Update docs | `/6_doc` |
| `/7_go` | Ad-hoc work (in a worktree by default) | `/7_go <specific task>` |
| `/8_done` | Session wrap-up + worktree cleanup | `/8_done` |
| `/9_deploy` | Production deploy (first-run setup or merge-to-main) | `/9_deploy` |

### Typical Workflow

```
1. Project Setup
   /1_start "Build a task management app"

2. Phase Planning
   /2_pm "User authentication"

3. Implementation (each in its own worktree, repeat)
   /3_dev
   /3_dev
   /3_dev

4. Functional Validation
   /4_test

5. Visual Validation (if UI work)
   /5_visual

6. Documentation
   /6_doc

7. Next Phase
   /2_pm "Task CRUD operations"
   (repeat 3-6)

Session Wrap-up
   /8_done

Ad-hoc Work (anytime, in its own worktree)
   /7_go "Fix login bug"

Release to Production
   /9_deploy
   (first run: walks through Mac PROD setup; subsequent: merges + watches)
```

---

## Architecture

**System Flow:**
```
[COMPONENT_1] → [COMPONENT_2] → [COMPONENT_3]
```

**Key Components:**
- **[COMPONENT_1]**: [PURPOSE]
- **[COMPONENT_2]**: [PURPOSE]
- **[COMPONENT_3]**: [PURPOSE]

**Data Flow:**
[DESCRIBE_HOW_DATA_MOVES_THROUGH_SYSTEM]

**For full architecture:** See `docs/2_ARCHITECTURE.md`

---

## Design System

**Colors:** [PRIMARY], [SECONDARY], [ACCENT]  
**Typography:** [FONT_1] (headings), [FONT_2] (body)  
**Spacing:** [SPACING_SYSTEM]

**Guidelines:**
- Use defined colors (not arbitrary values)
- Use typography scale (not random sizes)
- Use spacing system (not magic numbers)

**For full design system:** See `docs/3_DESIGN_SYSTEM.md`

---

## Quality Standards

**Test Coverage:** [X%] minimum  
**Performance:** [METRIC_TARGET]  
**Security:** [REQUIREMENTS]

**Testing:**
- Unit tests for all code
- Integration tests for APIs
- E2E tests for user flows
- All tests must pass

**For full QA strategy:** See `docs/4_QUALITY_ASSURANCE.md`

---

## Development Tools

### MCP Servers

This project uses Model Context Protocol (MCP) servers for enhanced development capabilities.

#### Sequential Thinking
- Complex problem-solving
- Architecture planning
- Trade-off analysis

#### Playwright
- Browser automation
- Frontend testing
- Visual regression
- Console error detection

**For MCP details:** See `CLAUDE.md`

---

## Development Guidelines

### Code Quality
- Follow `2_ARCHITECTURE.md` patterns
- Use `3_DESIGN_SYSTEM.md` standards
- Meet `4_QUALITY_ASSURANCE.md` requirements
- Write tests for new code
- Add type annotations
- Handle errors gracefully

### Git Commits
```bash
git commit -m "[type](scope): description"
```
**Types:** feat, fix, refactor, test, docs, chore

### Documentation
- Update existing docs
- Keep HANDOFF_NOTES.md current
- Document decisions
- Update code examples

---

## Commands

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

**Setup:** Copy `.env.example` to `.env` and configure

---

## Project Structure

```
[PROJECT_NAME]/
├── src/                  # Source code
├── tests/                # Test suites
├── docs/                 # Documentation
├── config/               # Configuration
└── scripts/              # Utility scripts
```

---

## Getting Started

### Prerequisites
- [REQUIREMENT_1]
- [REQUIREMENT_2]
- [REQUIREMENT_3]

### Installation
```bash
# Clone
git clone [REPO_URL]
cd [PROJECT_NAME]

# Install
[INSTALL_COMMAND]

# Configure
cp .env.example .env

# Start
[START_COMMAND]
```

### Verify Installation
```bash
# Check services
[CHECK_COMMAND]

# Run tests
[TEST_COMMAND]

# Access app
open [APP_URL]
```

---

## Troubleshooting

**Services won't start:**
- Check: [SERVICE_CHECK_COMMAND]
- Verify: [REQUIREMENT]
- Logs: [LOG_COMMAND]

**Tests failing:**
- Check test output
- Verify environment
- Review recent changes
- Check `docs/HANDOFF_NOTES.md`

**Need context:**
- Read `docs/HANDOFF_NOTES.md`
- Check `git log --oneline -20`
- Review current phase tasks

---

## Contributing

1. Read `docs/1_PROJECT_OVERVIEW.md` for project vision
2. Review `docs/2_ARCHITECTURE.md` for patterns
3. Check current phase in `docs/Phase [N]/`
4. Follow development workflow above
5. Write tests
6. Update documentation

---

## Resources

### Documentation
- [1_PROJECT_OVERVIEW.md](docs/1_PROJECT_OVERVIEW.md) - Vision & goals
- [2_ARCHITECTURE.md](docs/2_ARCHITECTURE.md) - Technical architecture
- [3_DESIGN_SYSTEM.md](docs/3_DESIGN_SYSTEM.md) - UI/UX guidelines
- [4_QUALITY_ASSURANCE.md](docs/4_QUALITY_ASSURANCE.md) - Testing strategy

### Development
- [CLAUDE.md](CLAUDE.md) - AI agent guide
- [HANDOFF_NOTES.md](docs/HANDOFF_NOTES.md) - Session tracking
- Phase docs in `docs/Phase [N]/`

---

## Quick Reference

**Get project context:**
```bash
cat docs/HANDOFF_NOTES.md          # Recent work
git log --oneline -10               # Recent commits
git status                          # Current changes
```

**Find code:**
```bash
grep -r "[PATTERN]" src/            # Search codebase
```

**Check standards:**
```bash
cat docs/2_ARCHITECTURE.md          # Architecture patterns
cat docs/3_DESIGN_SYSTEM.md         # Design standards
cat docs/4_QUALITY_ASSURANCE.md     # Quality requirements
```

**Phase progress:**
```bash
# Find latest phase
ls docs/Phase\ */ -d

# Check tasks
cat "docs/Phase [N]/PHASE_TASKS.md"
```

---

## License

[LICENSE_TYPE]

---

## Support

[SUPPORT_INFORMATION]

---

**Built with a structured, phase-based development approach**
