# Project Kickoff - Initialize New Project

**Usage:** `/1_start <project description or problem to solve>`

---

## PHASE 1: DISCOVERY

Acknowledge and ask 2 questions **in one message**:

```
🚀 PROJECT KICKOFF

Description: [USER_INPUT]

Let me understand the context...

Q1: Is this a NEW project or EXISTING codebase?
A) New project (I'll help design it)
B) Existing codebase (provide path: _______)

Q2: Target quality level?
A) Quick prototype (speed over quality)
B) MVP (balanced, testable)
C) Production-ready (polished, tested)
D) Enterprise (comprehensive testing/docs)
```

**Wait for user's answers to both Q1 and Q2.**

---

## PHASE 2: DEEP EXPLORATION

**Branch based on Q1 answer (New vs Existing):**

---

### PATH A: EXISTING CODEBASE (Q1 = B)

**Automated Exploration - No Additional Questions**

```
📂 CODEBASE EXPLORATION

Location: [USER_PROVIDED_PATH]

Analyzing codebase automatically...
```

---

**Step 1: Analyze Structure (Automated)**
```bash
# Explore directory structure
tree -L 3 -I 'node_modules|dist|build|.git' [PATH]

# Or if tree not available
find [PATH] -maxdepth 3 -type d | grep -v 'node_modules\|dist\|build\|.git'

# Count files by type
find [PATH] -type f | sed 's/.*\.//' | sort | uniq -c | sort -rn | head -10
```

**Output:**
```
📊 Codebase Statistics:
- Total files: [COUNT]
- Total lines: [COUNT]
- Primary language: [LANGUAGE] ([PERCENTAGE]%)
- Secondary language: [LANGUAGE] ([PERCENTAGE]%)

📁 Directory Structure:
[TREE_OUTPUT]
```

**Step 3: Identify Tech Stack**
```bash
# Check for package files
ls -la package.json requirements.txt Gemfile pom.xml build.gradle go.mod Cargo.toml composer.json 2>/dev/null

# Read dependencies
cat package.json | grep -A 50 '"dependencies"' 2>/dev/null
cat requirements.txt 2>/dev/null
cat go.mod 2>/dev/null

# Check runtime versions
cat .node-version .nvmrc .python-version .ruby-version 2>/dev/null
```

```
🔍 TECH STACK DISCOVERED

Frontend: [FRAMEWORK/LIBRARY + VERSION]
Backend: [FRAMEWORK/LANGUAGE + VERSION]
Database: [TYPE] (found in: [WHERE])
Package Manager: [npm/yarn/pip/etc]
Build Tool: [webpack/vite/etc]
Testing: [jest/pytest/etc]

Key Dependencies:
- [DEPENDENCY_1]: v[VERSION]
- [DEPENDENCY_2]: v[VERSION]
- [DEPENDENCY_3]: v[VERSION]
```

---

**Step 3: Analyze Architecture Patterns (Automated)**
```bash
# Find common patterns
grep -r "export class" [PATH]/src | wc -l  # Classes
grep -r "export function" [PATH]/src | wc -l  # Functions
grep -r "export const.*= {" [PATH]/src | wc -l  # Objects

# Find architectural files
find [PATH] -name "*service*" -o -name "*controller*" -o -name "*model*" -o -name "*repository*" | head -20

# Check for common patterns
ls -la [PATH]/src/components 2>/dev/null
ls -la [PATH]/src/services 2>/dev/null
ls -la [PATH]/src/models 2>/dev/null
ls -la [PATH]/backend/api 2>/dev/null
```

```
🏗️ ARCHITECTURE PATTERNS

Project Structure:
- Pattern: [MVC/MVVM/Clean/Hexagonal/Layered]
- Components: [LIST_KEY_DIRECTORIES]

Key Patterns Found:
- Services: [COUNT] files in [LOCATION]
- Controllers: [COUNT] files in [LOCATION]
- Models: [COUNT] files in [LOCATION]
- Components: [COUNT] files in [LOCATION]

Naming Conventions:
- Files: [camelCase/kebab-case/PascalCase]
- Classes: [PascalCase/etc]
- Functions: [camelCase/etc]
```

---

**Step 4: Find Configuration & Environment (Automated)**
```bash
# Configuration files
ls -la .env* config/ *.config.js *.config.ts tsconfig.json 2>/dev/null

# Docker setup
ls -la Dockerfile docker-compose.yml .dockerignore 2>/dev/null

# CI/CD
ls -la .github/workflows .gitlab-ci.yml .circleci/ 2>/dev/null
```

```
⚙️ CONFIGURATION & SETUP

Environment:
- Config files: [LIST]
- Environment variables: [COUNT] (from .env.example)

Infrastructure:
- Docker: [YES/NO]
- CI/CD: [PLATFORM]
- Hosting: [DETECTED_FROM_CONFIG]

Build Process:
- Dev: [COMMAND]
- Build: [COMMAND]
- Test: [COMMAND]
```

---

**Step 5: Examine Existing Tests (Automated)**
```bash
# Find test files
find [PATH] -name "*.test.*" -o -name "*.spec.*" -o -name "*_test.*" | wc -l

# Check test directories
ls -la tests/ test/ __tests__/ 2>/dev/null

# Test coverage config
cat .coveragerc jest.config.js vitest.config.ts 2>/dev/null | grep -i coverage
```

```
🧪 TESTING SETUP

Test Framework: [FRAMEWORK]
Test Files: [COUNT]
Test Location: [PATHS]
Coverage Target: [PERCENTAGE]% (if configured)
E2E Tests: [YES/NO] ([FRAMEWORK])
```

---

**Step 6: Read Key Files for Context (Automated)**
```bash
# Project documentation
cat README.md 2>/dev/null | head -50
cat CONTRIBUTING.md 2>/dev/null | head -30
cat CHANGELOG.md 2>/dev/null | head -30

# Main entry points
cat package.json | grep '"main"\|"start"\|"dev"'
find [PATH] -name "main.*" -o -name "index.*" -o -name "app.*" | head -5
```

---

**Step 7: Identify Integration Points (Automated)**
```bash
# Find API endpoints
grep -r "app.get\|app.post\|@app.route\|@RestController" [PATH] | head -20

# Find database connections
grep -r "mongoose.connect\|createConnection\|Pool\|Sequelize" [PATH] | head -10

# Find external services
grep -r "axios\|fetch\|requests.get\|http.client" [PATH] | grep -v node_modules | head -10
```

---

**Step 8: Present Complete Analysis**
```
✅ CODEBASE ANALYSIS COMPLETE

📊 Codebase Summary:
- Location: [PATH]
- Language: [PRIMARY_LANGUAGE]
- Framework: [FRAMEWORK]
- Architecture: [PATTERN]
- Total files: [COUNT]
- Lines of code: [COUNT]

🔧 Tech Stack:
- Frontend: [FRAMEWORK/LIBRARY + VERSION]
- Backend: [FRAMEWORK/LANGUAGE + VERSION]
- Database: [TYPE]
- Testing: [FRAMEWORK]

🏗️ Architecture:
- Pattern: [PATTERN]
- Key directories: [LIST]
- Main entry: [FILE]

🎯 Integration Points:
- API Endpoints: [COUNT] found
- Database connections: [TYPE]
- External services: [LIST]

🧪 Testing:
- Test files: [COUNT]
- Framework: [FRAMEWORK]
- Coverage: [PERCENTAGE]% (if found)

📋 Documentation Quality:
- README: [GOOD/MODERATE/POOR/MISSING]
- API docs: [GOOD/MODERATE/POOR/MISSING]
- Comments: [EXTENSIVE/MODERATE/MINIMAL]

---

I'll now create comprehensive documentation based on this analysis.
Ready to proceed? Reply:
- "yes" or "proceed" → I'll generate all docs
- Or tell me if I missed anything or got something wrong
```

**Wait for user confirmation. If issues noted, investigate further. Then proceed.**

---

### PATH B: NEW PROJECT (Q1 = A)

**Step 1: Gather Requirements (Single Conversational Prompt)**
```
🎯 PROJECT DESIGN

I need to understand your project to recommend the right architecture.

Please tell me about:

1. **Core Problem:** What problem does this solve? (1-2 sentences)

2. **Primary Users:** Who will use this?
   - Developers / End consumers / Business users / Internal automation / Other

3. **Main User Actions:** What will users primarily do?
   - Examples: "Create and manage tasks", "Search and filter products", "Real-time chat"

4. **Tech Preferences (optional):** Any specific technologies you want/need to use?
   - If none, I'll recommend based on requirements

5. **Key Constraints (optional):** Scale expectations, performance needs, must-haves?
```

**Wait for user's comprehensive response.**

---

**Step 2: Analyze & Design (AI Does This Automatically)**

Based on user's response, AI should:

1. **Infer requirements:**
   - Data complexity (from problem description)
   - Scale needs (from user type and actions)
   - Real-time requirements (from actions described)
   - Interface type (from user type and actions)

2. **Make tech stack recommendations:**
   - Frontend: [FRAMEWORK] - justified by requirements
   - Backend: [FRAMEWORK] - justified by requirements
   - Database: [DATABASE] - justified by data needs
   - Infrastructure: [APPROACH] - justified by scale

3. **Design data model:**
   - Core entities (2-5 main ones)
   - Key relationships
   - Essential fields

4. **Define API endpoints (if applicable):**
   - RESTful CRUD operations
   - Custom endpoints for main actions
   - Authentication approach

5. **Identify MVP features:**
   - Must-have (3-5 features)
   - Nice-to-have (2-3 features)

6. **Note risks:**
   - Technical challenges
   - Scope concerns
   - Timeline considerations

---

**Step 3: Present Complete Design**
```
✅ PROJECT DESIGN COMPLETE

🎯 Problem: [ONE_SENTENCE]
👥 Primary Users: [USER_TYPE]

🔧 Recommended Tech Stack:
- Frontend: [FRAMEWORK] (why: [REASON])
- Backend: [FRAMEWORK] (why: [REASON])
- Database: [DATABASE] (why: [REASON])
- Infrastructure: [DOCKER/K8S/SERVERLESS] (why: [REASON])

📊 Data Model:
- [ENTITY_1]: [key_fields]
- [ENTITY_2]: [key_fields]
- [ENTITY_3]: [key_fields]
- Relationships: [SUMMARY]

🔌 API Design:
- [METHOD] /api/v1/[resource]
- [METHOD] /api/v1/[resource]/{id}
- [CUSTOM_ENDPOINTS]
- Auth: [METHOD]

✨ MVP Feature Scope:
Must Have:
- [FEATURE_1]
- [FEATURE_2]
- [FEATURE_3]

Nice to Have:
- [FEATURE_4]
- [FEATURE_5]

⚠️ Key Risks:
- [RISK_1]: [MITIGATION]
- [RISK_2]: [MITIGATION]

📅 Estimated MVP Timeline: [WEEKS]

---

Does this design work for you? Reply:
- "yes" or "proceed" → I'll create documentation
- Or tell me what to adjust (tech stack, features, scope, etc.)
```

**Wait for user confirmation or adjustments. Make changes if requested, then proceed.**

---

## PHASE 3: ARCHITECTURAL DECISION RECORDS

**For both paths, document key decisions:**

```bash
# Create ADR directory
mkdir -p docs/decisions
```

**Create decision log:**
```markdown
# docs/decisions/000-index.md

# Architectural Decision Records

## Purpose
Document key architectural decisions made during project setup.

## Decisions

| # | Title | Date | Status |
|---|-------|------|--------|
| 001 | Tech Stack Selection | [DATE] | Accepted |
| 002 | Database Choice | [DATE] | Accepted |
| 003 | [DECISION_3] | [DATE] | Accepted |
```

**For each major decision:**
```markdown
# docs/decisions/001-tech-stack-selection.md

# ADR 001: Tech Stack Selection

**Date:** [DATE]
**Status:** Accepted

## Context
[Why this decision needed to be made]

## Decision
We chose [STACK] because:
- [REASON_1]
- [REASON_2]
- [REASON_3]

## Alternatives Considered
- [ALT_1]: Rejected because [REASON]
- [ALT_2]: Rejected because [REASON]

## Consequences
**Positive:**
- [BENEFIT_1]
- [BENEFIT_2]

**Negative:**
- [TRADE_OFF_1]
- [TRADE_OFF_2]

**Neutral:**
- [CONSIDERATION_1]
```

---

## PHASE 4: CONSOLIDATE & PLAN

**AI automatically creates this summary (no user input needed):**

```
📋 PROJECT FOUNDATION READY

Project: [PROJECT_NAME]
Type: [NEW PROJECT | EXISTING CODEBASE]
Quality Target: [Q2_ANSWER]

[IF EXISTING CODE]
🔍 Codebase:
- Location: [PATH]
- Stack: [LANGUAGE/FRAMEWORK]
- Architecture: [PATTERN]
- Files: [COUNT] | Tests: [COUNT]

[IF NEW PROJECT]
🎯 Design:
- Problem: [ONE_SENTENCE]
- Users: [USER_TYPE]
- Stack: [SUMMARY]
- MVP Features: [COUNT]

🏗️ Architecture:
- Pattern: [PATTERN_NAME]
- Frontend: [FRAMEWORK]
- Backend: [FRAMEWORK]
- Database: [DATABASE]

📄 Creating Documentation Structure:

Root:
- CLAUDE.md (AI agent guide)
- README.md (quick start)

docs/:
- 1_PROJECT_OVERVIEW.md (vision & goals)
- 2_ARCHITECTURE.md (technical architecture)
- 3_DESIGN_SYSTEM.md (UI/UX standards)
- 4_QUALITY_ASSURANCE.md (testing strategy)
- HANDOFF_NOTES.md (session tracking)
- decisions/ (architectural decisions)
  ├─ 000-index.md
  ├─ 001-tech-stack.md
  ├─ 002-database.md
  └─ [ADDITIONAL ADRs]

Note: Use /2_pm later to plan implementation phases

Proceeding with documentation generation...
```

**No user confirmation needed - proceed automatically to PHASE 5.**

---

## PHASE 5: EXECUTE

### Copy Templates

```bash
cp templates/CLAUDE.md ./CLAUDE.md
cp templates/README.md ./README.md
mkdir -p docs
cp templates/docs/*.md docs/
mkdir -p docs/decisions
cp templates/docs/decisions/*.md docs/decisions/
```

### Create ADRs First

**Create 3-5 Architectural Decision Records based on PHASE 3:**

```
📝 Creating ADRs from design decisions...
```

**1. Create index:**
```markdown
docs/decisions/000-index.md
[List all decisions made]
```

**2-5. Create individual ADRs:**
```markdown
docs/decisions/001-tech-stack-selection.md
docs/decisions/002-database-choice.md
docs/decisions/003-[ADDITIONAL_DECISION].md
```

Each ADR includes:
- Context (why decision needed)
- Decision (what was chosen)
- Alternatives considered
- Consequences (pros/cons/trade-offs)

### Update 7 Core Documents (One at a Time)

For each document:
```
📝 [DOCUMENT_NAME]
[UPDATE WITH REAL CONTENT FROM PHASE 2 EXPLORATION]
✅ Done
```

**Update Order:**
1. **CLAUDE.md** - Overview, tech stack, commands, patterns
   - Use findings from codebase analysis OR POC design
   - Include actual file paths and commands
   - Reference ADRs for key decisions

2. **1_PROJECT_OVERVIEW.md** - Vision, goals, users, features, scope
   - Use requirements gathered for new projects
   - Document existing project's purpose for codebases
   - Clear success metrics

3. **2_ARCHITECTURE.md** - Components, data flow, database, APIs
   - Use actual architecture patterns found OR designed
   - Include real component names and locations
   - Diagram data flow
   - Document API endpoints
   - Reference ADRs

4. **3_DESIGN_SYSTEM.md** - Colors, typography, components
   - Extract from existing codebase CSS/styles OR
   - Define for new POC based on requirements
   - Skip if CLI/API-only project

5. **4_QUALITY_ASSURANCE.md** - Test strategy, coverage, CI/CD
   - Use existing test framework found OR
   - Define testing approach for POC
   - Set coverage targets based on Q2 (quality level)

6. **HANDOFF_NOTES.md** - Initialize Session 0, note decisions
   - Document PHASE 2 exploration findings
   - List key decisions made
   - Note any concerns or risks
   - Record exploration time

7. **README.md** - Quick start, installation, usage
   - Use actual setup commands
   - Include prerequisites discovered/defined
   - Working code examples

---

## PHASE 6: FINALIZE

**Auto-Validate:**
```bash
# Check for unfilled placeholders (excluding Phase [N] which is intentional)
echo "🔍 Validating documentation..."

PLACEHOLDERS=$(grep -r "\[.*\]" CLAUDE.md README.md docs/*.md docs/decisions/*.md 2>/dev/null | \
  grep -v "Phase \[N\]" | \
  grep -v "\.git" | \
  grep -v "\[x\]" | \
  grep -v "\[ \]" | \
  grep -v "\[~\]" | \
  grep -v "\[N\]" | \
  wc -l | tr -d ' ')

if [ "$PLACEHOLDERS" -gt 0 ]; then
  echo "❌ VALIDATION FAILED: Found $PLACEHOLDERS unfilled placeholders"
  echo ""
  echo "Unfilled placeholders:"
  grep -rn "\[.*\]" CLAUDE.md README.md docs/*.md docs/decisions/*.md 2>/dev/null | \
    grep -v "Phase \[N\]" | \
    grep -v "\.git" | \
    grep -v "\[x\]" | \
    grep -v "\[ \]" | \
    grep -v "\[~\]" | \
    grep -v "\[N\]" | \
    head -10
  echo ""
  echo "⚠️  Please replace all [PLACEHOLDERS] with actual content"
  exit 1
fi

echo "✅ Validation passed: No placeholders found"
echo "✅ Tech stack consistency: Checking..."

# Verify tech stack mentioned consistently
TECH_STACK_DOCS=$(grep -l "Tech Stack" CLAUDE.md docs/2_ARCHITECTURE.md 2>/dev/null | wc -l | tr -d ' ')
if [ "$TECH_STACK_DOCS" -lt 2 ]; then
  echo "⚠️  Warning: Tech stack should be documented in both CLAUDE.md and 2_ARCHITECTURE.md"
fi

# Verify ADRs exist
ADR_COUNT=$(find docs/decisions -name "*.md" 2>/dev/null | wc -l | tr -d ' ')
if [ "$ADR_COUNT" -lt 2 ]; then
  echo "⚠️  Warning: Expected at least 2 ADRs (index + 1 decision)"
else
  echo "✅ Found $ADR_COUNT architectural decision records"
fi

echo "✅ All validation checks passed!"
```

**Manual Validate:**
- ✅ No [PLACEHOLDERS] remain (auto-checked above)
- ✅ Tech stack consistent across docs
- ✅ Architecture matches in CLAUDE.md & 2_ARCHITECTURE.md
- ✅ README has working commands
- ✅ HANDOFF_NOTES initialized with Session 0
- ✅ Quality appropriate for Q2 answer
- ✅ ADRs document key decisions
- ✅ Documentation reflects PHASE 2 exploration

**Output:**
```
✅ KICKOFF COMPLETE

📁 Structure:
./CLAUDE.md, README.md
./docs/
  ├─ 1_PROJECT_OVERVIEW.md
  ├─ 2_ARCHITECTURE.md
  ├─ 3_DESIGN_SYSTEM.md (if created)
  ├─ 4_QUALITY_ASSURANCE.md
  ├─ HANDOFF_NOTES.md
  └─ decisions/
      ├─ 000-index.md
      ├─ 001-tech-stack-selection.md
      ├─ 002-database-choice.md
      └─ [MORE_AS_NEEDED]

📊 Summary:
- Name: [PROJECT_NAME]
- Type: [PROJECT_TYPE]
- Stack: [STACK_SUMMARY]
- ADRs created: [COUNT]

[IF EXISTING CODE]
🔍 Exploration Results:
- Files analyzed: [COUNT]
- Language: [PRIMARY_LANGUAGE]
- Framework: [FRAMEWORK]
- Architecture pattern: [PATTERN]
- Tests found: [COUNT]
- Documentation quality: [GOOD/MODERATE/POOR]

[IF NEW POC]
🎯 Design Results:
- Target users: [USER_TYPE]
- Expected scale: [USER_SCALE]
- Interface: [TYPE]
- MVP features: [COUNT]
- Entities designed: [COUNT]
- Risks identified: [COUNT]

⏱️  Exploration time: [MINUTES] minutes
📋 Decisions documented: [COUNT] ADRs
🎯 Ready for: Phase planning

🎯 Next: Use /2_pm to plan Phase 1
```

---

## GUIDELINES

**Streamlined Approach:**
- **Minimize user friction:** Ask only essential questions
- **Make smart inferences:** Don't ask what can be deduced from context
- **Default to action:** Present recommendations vs asking permissions
- **Automate exploration:** Run analysis commands without asking
- **Batch confirmations:** One approval point instead of multiple

**Exploration Depth:**
- **Existing Codebase:** Be thorough and automated
  - Analyze actual file structure, don't guess
  - Find patterns by example, not assumption
  - Document what exists, not what should exist
  - Read key files for real context
  - Count actual metrics (files, tests, endpoints)
  - Run ALL exploration steps automatically
  - Present complete findings once

- **New Project:** Be conversational and efficient
  - Gather requirements in ONE comprehensive prompt
  - Infer technical needs from problem description
  - Provide justified recommendations (not multiple choice questions)
  - Design data model based on requirements
  - Identify risks proactively
  - Keep MVP scope realistic
  - Present complete design for single approval

**Content Quality:**
- Write for AI agents (clear, actionable, scannable)
- Use REAL examples from exploration (not placeholders)
- Include WORKING commands from codebase
- Reference CONCRETE file paths found/created
- Document ACTUAL decisions made
- Capture WHY, not just WHAT

**Templates:**
- Keep structure intact
- Fill ALL sections (use exploration findings)
- Match heading levels
- Preserve format
- NO PLACEHOLDERS in final docs

**ADR Best Practices:**
- Create ADR for each major decision
- Include context, decision, alternatives, consequences
- Be honest about trade-offs
- Reference specific requirements gathered during discovery
- Update status if decision changes later

**Scope Management:**
- Focus on foundation & architecture (not tasks)
- Don't break into implementation tasks (that's /pm's job)
- Ensure design supports future phases
- **Scale effort to Q2 quality level:**
  - **Quick prototype:** Minimal docs -- CLAUDE.md, README.md, and a light 2_ARCHITECTURE.md may be enough. Skip 3_DESIGN_SYSTEM.md and detailed ADRs. Keep it lean.
  - **MVP:** Standard doc set. Fill core docs but keep them concise. 1-2 ADRs for key decisions.
  - **Production-ready:** Full doc set, thorough ADRs, detailed architecture and quality standards.
  - **Enterprise:** Deep analysis, comprehensive ADRs, detailed quality gates and security considerations.

**Interaction Flow Summary:**
```
Total user interactions: 3-4 (down from 14)

1. Initial questions (2 combined): Project type + Quality level
2. [Path A] Exploration findings → Confirm
   [Path B] Requirements prompt → Design → Confirm  
3. Auto-generate all documentation
4. Present final summary

Maximum questions asked: 2 upfront + 1 comprehensive + 1 confirmation = 4 total
```

**Troubleshooting:**
- Vague description → Use conversational prompt to gather details naturally
- Large existing codebase → Focus on key patterns, don't analyze everything
- Unclear tech preference → Make smart recommendations with justification
- Missing info in codebase → Document gaps as risks
- Can't find pattern → Note as "undefined/ad-hoc" in docs
- User objects to design → Adjust and re-present (don't re-ask all questions)

---

**Input:** [USER_PROJECT_DESCRIPTION]  
**Start at:** PHASE 1: DISCOVERY
