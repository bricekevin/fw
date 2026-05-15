# Go - Ad-Hoc Development Task

**Usage:** `/7_go <what to work on>`

**Purpose:** Flexible, user-directed development that follows project standards

---

## STEP 0: WORKTREE SETUP (default — don't ask)

Ad-hoc work still gets its own worktree by default so a parallel session can keep working on the main branch / other branches. See `CLAUDE.md` → "Parallel Sessions & Worktrees".

```bash
CURRENT_DIR=$(git rev-parse --show-toplevel 2>/dev/null)
MAIN_WORKTREE=$(git worktree list 2>/dev/null | head -n 1 | awk '{print $1}')

if [ -z "$CURRENT_DIR" ]; then
  echo "Not in a git repo — skipping worktree setup."
elif [ "$CURRENT_DIR" != "$MAIN_WORKTREE" ]; then
  echo "Already in worktree: $CURRENT_DIR — continuing here."
else
  REPO_NAME=$(basename "$CURRENT_DIR")
  # Derive a short slug from the task description ($USER_DESCRIPTION).
  # e.g., "Add dark mode toggle" -> "go-dark-mode-toggle"
  SLUG=$(echo "$USER_DESCRIPTION" | tr '[:upper:]' '[:lower:]' \
    | sed 's/[^a-z0-9]/-/g; s/--*/-/g; s/^-//; s/-$//' | cut -c1-40)
  BRANCH="go-${SLUG:-$(date +%Y%m%d-%H%M)}"
  WORKTREE_PATH="../${REPO_NAME}-${BRANCH}"

  if [ ! -d "$WORKTREE_PATH" ]; then
    git fetch origin
    DEFAULT_BRANCH=$(git symbolic-ref refs/remotes/origin/HEAD 2>/dev/null | sed 's@^refs/remotes/origin/@@' || echo "main")
    git worktree add -b "$BRANCH" "$WORKTREE_PATH" "origin/$DEFAULT_BRANCH"
  fi

  echo "Switching to worktree: $WORKTREE_PATH"
  # Subsequent steps assume cwd == worktree
fi
```

**Skip STEP 0 only when:** the task is read-only (exploration, explanation), or the user explicitly asks to stay in the main checkout (e.g., a quick fix on `main` they want to push immediately).

---

## STEP 1: UNDERSTAND THE TASK

**User requested:**
```
[USER_DESCRIPTION]
```

**Clarify if needed:**
- What specific feature/fix/change?
- Which part of the system?
- Any constraints or preferences?
- Expected outcome?

---

## STEP 2: LOAD FULL PROJECT CONTEXT

```bash
# Read project foundation
cat CLAUDE.md                       # Project overview & commands
cat docs/1_PROJECT_OVERVIEW.md      # Goals & scope
cat docs/2_ARCHITECTURE.md          # Tech stack & patterns
cat docs/3_DESIGN_SYSTEM.md         # UI/UX styles (if exists)
cat docs/4_QUALITY_ASSURANCE.md     # Testing standards
cat docs/HANDOFF_NOTES.md           # Recent work & context

# Check project state
git status
git log --oneline -10

# Check environment (from CLAUDE.md)
[CHECK_SERVICES_COMMAND]
```

**Understand:**
- Project tech stack
- Existing patterns to follow
- Recent work done
- Current system state
- Quality standards to meet

---

## STEP 3: LOCATE RELEVANT CODE

**Based on task, find related:**

```bash
# Search for related functionality
grep -r "[KEYWORD]" src/ backend/ frontend/

# Find similar patterns
grep -r "class [PATTERN]" 
grep -r "function [PATTERN]"

# Check existing implementations
ls -la [RELEVANT_DIRECTORY]
```

**Review:**
- Existing similar code
- Patterns to follow from 2_ARCHITECTURE.md
- Related components
- Tests for similar features

---

## STEP 4: PLAN APPROACH

**Break down the work:**

```
📋 IMPLEMENTATION PLAN

1. [STEP_1]
   Files: [FILES_TO_MODIFY]
   Pattern: [PATTERN_TO_FOLLOW]

2. [STEP_2]
   Files: [FILES_TO_MODIFY]
   Pattern: [PATTERN_TO_FOLLOW]

3. [STEP_3]
   Files: [FILES_TO_MODIFY]
   Tests: [TESTS_TO_WRITE]

4. Verify & Test
   Commands: [TEST_COMMANDS]
```

**Confirm approach with user before proceeding if unclear**

---

## STEP 5: IMPLEMENT

**For each step:**

### 5.1 Write Code

**Follow project standards:**
- **Architecture:** Match patterns from 2_ARCHITECTURE.md
- **Tech Stack:** Use project's chosen technologies
- **Styles:** Apply 3_DESIGN_SYSTEM.md (colors, typography, spacing)
- **Quality:** Meet standards from 4_QUALITY_ASSURANCE.md

**Code guidelines:**
```
✅ Follow existing patterns
✅ Use design system tokens (not arbitrary values)
✅ Match project structure
✅ Add type annotations (if typed language)
✅ Handle errors properly
✅ Add logging where appropriate
```

### 5.2 Write Tests

```bash
# Write unit tests for new code
[TEST_FILE]

# Run tests
[TEST_COMMAND_FROM_CLAUDE_MD]
```

**Tests must:**
- Cover new functionality
- Pass before committing
- Meet coverage targets from 4_QUALITY_ASSURANCE.md

### 5.3 Verify

```bash
# Run the code
[RUN_COMMAND]

# Check logs
[LOG_COMMAND]

# Test manually
[MANUAL_TEST_STEPS]
```

### 5.4 Commit

```bash
git add [FILES]
git commit -m "[type](scope): [description]

- [change 1]
- [change 2]"
```

**Commit types:** feat, fix, refactor, test, docs, chore

---

## STEP 6: TEST COMPREHENSIVELY

**Run all relevant tests:**

```bash
# Unit tests
[UNIT_TEST_COMMAND]

# Integration tests (if applicable)
[INTEGRATION_TEST_COMMAND]

# E2E tests (if applicable)
[E2E_TEST_COMMAND]

# Linting
[LINT_COMMAND]
```

**Verify:**
- ✅ All tests pass
- ✅ No linting errors
- ✅ Coverage meets targets
- ✅ No console errors (if UI)

---

## STEP 7: UPDATE DOCUMENTATION

**Update relevant docs:**

### If feature adds new functionality:
```
docs/2_ARCHITECTURE.md - Add component/API
docs/3_DESIGN_SYSTEM.md - Document new UI patterns
README.md - Update feature list
```

### If modifying existing phase:
```
docs/Phase [N]/PHASE_IMP.md - Update implementation details
docs/Phase [N]/PHASE_TASKS.md - Check off related tasks
```

### Always update:
```
docs/HANDOFF_NOTES.md - Add session entry
```

**Handoff entry:**
```markdown
## Session [N] - [DATE]

**Agent:** [NAME]
**Task:** [USER_REQUEST]
**Duration:** [TIME]

### Completed
- [WHAT_WAS_DONE]

### Files Changed
- `[FILE_1]`: [CHANGES]
- `[FILE_2]`: [CHANGES]

### Testing
- Unit tests: [PASS/FAIL]
- Manual testing: [RESULTS]

### Known Issues
- [ISSUE_IF_ANY]

### Notes
[CONTEXT_FOR_NEXT_SESSION]
```

---

## STEP 8: OUTPUT SESSION SUMMARY

```
✅ TASK COMPLETE

📋 Request:
[USER_DESCRIPTION]

✅ Completed:
- [ACHIEVEMENT_1]
- [ACHIEVEMENT_2]
- [ACHIEVEMENT_3]

📝 Files Changed:
- [FILE_1]
- [FILE_2]
- [FILE_3]

🧪 Testing:
- Unit tests: ✅ PASSING
- Coverage: [X%]
- Manual verification: ✅ WORKING

📄 Docs Updated:
- [DOC_1]
- [DOC_2]
- HANDOFF_NOTES.md

🎯 Deliverable:
[WHAT_USER_CAN_NOW_DO]
```

---

## GUIDELINES

**Task Approach:**
- Understand requirements fully before coding
- Break into small, testable steps
- Follow existing patterns (don't reinvent)
- Prefer modifying over creating new files
- Commit frequently with clear messages

**Code Quality:**
- **Architecture:** Follow 2_ARCHITECTURE.md patterns
- **Design:** Use 3_DESIGN_SYSTEM.md tokens
- **Quality:** Meet 4_QUALITY_ASSURANCE.md standards
- **Consistency:** Match existing code style
- **Types:** Add type annotations
- **Errors:** Handle gracefully
- **Logs:** Add for debugging

**Testing:**
- Write tests for all new code
- Run tests before committing
- Meet coverage targets
- Verify manually
- Check logs for errors

**Documentation:**
- Update existing docs (don't create new)
- Keep HANDOFF_NOTES.md current
- Document decisions made
- Explain trade-offs

**Flexibility:**
- Not tied to PHASE_TASKS.md (unlike /3_dev)
- User directs the work
- Can work on anything in project
- Can be exploratory or experimental
- Still maintains project standards

---

## COMMON PATTERNS

**Quick code search:**
```bash
# Find similar functionality
grep -r "[PATTERN]" src/ backend/

# Find component/function definition
grep -rn "^export.*[NAME]" 
grep -rn "^class [NAME]"
grep -rn "^function [NAME]"
```

**Quick reference lookup:**
```bash
# Tech stack
grep -A 10 "Tech Stack:" docs/2_ARCHITECTURE.md

# Color palette
grep -A 20 "Color" docs/3_DESIGN_SYSTEM.md

# Test coverage target
grep -A 5 "Coverage" docs/4_QUALITY_ASSURANCE.md
```

**File structure:**
```bash
# List project structure
tree -L 3 -I 'node_modules|dist|build'

# Find files by name
find . -name "*[PATTERN]*" -type f
```

---

## TROUBLESHOOTING

**Unclear requirements:**
- Ask user for clarification
- Propose approach and get confirmation
- Show examples of what you plan to do

**Can't find existing patterns:**
- Check 2_ARCHITECTURE.md for guidance
- Look at similar features in codebase
- Ask user for preferred approach

**Tests failing:**
- Check test output carefully
- Verify environment is correct
- Review what changed
- Ask user to run tests if needed

**Don't know where to start:**
- Start with Step 2 (load context)
- Explore codebase (grep, find)
- Review similar features
- Ask user for guidance

---

## DIFFERENCES FROM /3_dev

| Aspect | /3_dev | /7_go |
|--------|--------|-------|
| **Task source** | PHASE_TASKS.md | User description |
| **Workflow** | Structured phase work | Flexible ad-hoc |
| **Scope** | Next task in sequence | User-specified |
| **Use case** | Phase implementation | Quick fixes, experiments |
| **Worktree** | One per phase branch | One per task slug |
| **Standards** | Same quality standards | Same quality standards |
| **Testing** | Required | Required |
| **Docs** | Updated | Updated |

**When to use /7_go:**
- Quick bug fix
- Experiment with feature
- Work outside current phase
- User has specific direction
- Ad-hoc improvements

**When to use /3_dev:**
- Working through phase tasks
- Structured implementation
- Following task checklist
- Sequential phase work

---

**Start:** STEP 0: WORKTREE SETUP, then STEP 1: UNDERSTAND THE TASK

**Example:**
```
/7_go Add a dark mode toggle to the settings page
/7_go Fix the pagination bug in the user list
/7_go Refactor the API client to use axios interceptors
/7_go Add validation to the login form
```
