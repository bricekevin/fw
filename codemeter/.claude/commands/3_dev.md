# Development Session - Implement Phase Tasks

**Usage:** `/3_dev [phase number]` or just `/3_dev` (defaults to latest phase)

---

## STEP 0: WORKTREE SETUP (default — don't ask)

This session writes code, so it gets its own git worktree by default. Another Claude session may be running against the same repo. Sibling-directory convention (see `CLAUDE.md` → "Parallel Sessions & Worktrees").

```bash
# Already in a worktree? (worktree shows main + linked checkouts)
IN_WORKTREE=$(git rev-parse --is-inside-work-tree 2>/dev/null)
GIT_DIR=$(git rev-parse --git-dir 2>/dev/null)
MAIN_WORKTREE=$(git worktree list | head -n 1 | awk '{print $1}')
CURRENT_DIR=$(git rev-parse --show-toplevel)

if [ "$CURRENT_DIR" != "$MAIN_WORKTREE" ]; then
  echo "Already in worktree: $CURRENT_DIR — continuing here."
else
  # Pick a branch name. Prefer phase-<N>-<slug>; otherwise short slug of task.
  REPO_NAME=$(basename "$CURRENT_DIR")
  PHASE_NUM="${1:-$(ls docs/Phase\ */ -d 2>/dev/null | sed 's/.*Phase //' | sed 's/\/.*//' | sort -n | tail -1)}"
  BRANCH="phase-${PHASE_NUM}-$(date +%Y%m%d)"   # tweak per task as needed
  WORKTREE_PATH="../${REPO_NAME}-${BRANCH}"

  if [ ! -d "$WORKTREE_PATH" ]; then
    git fetch origin
    if git show-ref --verify --quiet "refs/heads/$BRANCH"; then
      git worktree add "$WORKTREE_PATH" "$BRANCH"
    else
      # Branch off main (or develop, whichever this repo uses as default)
      DEFAULT_BRANCH=$(git symbolic-ref refs/remotes/origin/HEAD 2>/dev/null | sed 's@^refs/remotes/origin/@@' || echo "main")
      git worktree add -b "$BRANCH" "$WORKTREE_PATH" "origin/$DEFAULT_BRANCH"
    fi
  fi

  echo "Switching to worktree: $WORKTREE_PATH"
  echo "Run the rest of this session from there: cd \"$WORKTREE_PATH\""
  # Subsequent steps assume cwd == worktree
fi
```

**After this step, every subsequent `cd`, `git`, `pnpm install`, etc. happens inside the worktree.** Copy or symlink `.env*` files into the worktree if needed — they're not in git.

**Skip STEP 0 only if:** the user explicitly said "stay in the main checkout" or you're already inside a linked worktree.

---

## STEP 1: LOAD PROJECT CONTEXT

```bash
# Read project foundation
cat CLAUDE.md                       # Project overview & commands
cat docs/1_PROJECT_OVERVIEW.md      # Goals & scope
cat docs/2_ARCHITECTURE.md          # Tech stack & patterns
cat docs/3_DESIGN_SYSTEM.md         # UI/UX styles (if exists)
cat docs/4_QUALITY_ASSURANCE.md     # Testing standards
cat docs/HANDOFF_NOTES.md           # Last session notes

# Find phases
ls docs/Phase\ */ -d                # List all phases
```

**If user specified phase:** Use that  
**If not specified:** Find highest phase number automatically

```bash
# Auto-detect latest phase
LATEST_PHASE=$(ls docs/Phase\ */ -d | sed 's/.*Phase //' | sed 's/\/.*//' | sort -n | tail -1)
echo "Working on Phase $LATEST_PHASE"
```

---

## STEP 2: READ PHASE DOCS

```bash
# Load phase context
cat "docs/Phase $N/PHASE_PRD.md"    # What to build
cat "docs/Phase $N/PHASE_TASKS.md"  # Task checklist
cat "docs/Phase $N/PHASE_IMP.md"    # How to build
```

**Understand:**
- Phase goal from PRD
- Current task status ([ ] vs [x])
- Implementation patterns from IMP

---

## STEP 3: FIND NEXT TASK

**Scan PHASE_TASKS.md for:**
1. First `[~]` (in-progress) task → Continue that
2. No `[~]`? → Find first `[ ]` (pending) task → Start that
3. All `[x]`? → Phase complete!

**Print:**
```
📋 NEXT TASK

Epic: [EPIC_NAME]
Task: [TASK_NAME]
Status: [Starting new | Continuing in-progress]

Subtasks:
- [x] [Completed subtask]
- [~] [Current subtask]
- [ ] [Pending subtask]
```

---

## STEP 4: CHECK PROJECT STATE

```bash
# Git status
git status                        # Uncommitted work?
git log -3 --oneline             # Recent commits

# Environment check
[CHECK_IF_SERVICES_RUNNING]      # From CLAUDE.md
```

**Based on CLAUDE.md commands:**
- If Docker project → `docker ps`
- If local dev → Check process status
- If serverless → Check AWS/cloud status

---

## STEP 5: IMPLEMENT TASK

**For each subtask:**

1. **Update status**
   - Mark `[ ]` → `[~]` in PHASE_TASKS.md

2. **Implement**
   - **Architecture:** Follow patterns from 2_ARCHITECTURE.md
   - **Code Examples:** Reference PHASE_IMP.md
   - **Styles:** Use design tokens from 3_DESIGN_SYSTEM.md (colors, typography, spacing)
   - **Quality:** Meet standards from 4_QUALITY_ASSURANCE.md
   - **Tech Stack:** Match project's tech choices

3. **Test**
   - Run unit tests for code just written
   - Verify tests pass before committing
   - Run test commands from PHASE_IMP.md
   - Meet coverage targets from 4_QUALITY_ASSURANCE.md
   
   ```bash
   # Run unit tests
   [TEST_COMMAND_FROM_PHASE_IMP]
   
   # Verify coverage
   [COVERAGE_COMMAND]
   
   # All tests must pass before proceeding
   ```

4. **Commit**
   ```bash
   git add .
   git commit -m "feat(phase-$N): [task description]"
   ```

5. **Mark complete**
   - Change `[~]` → `[x]` in PHASE_TASKS.md
   - Only mark complete if tests pass

---

## STEP 6: UPDATE HANDOFF NOTES

**After completing task(s), update:** `docs/HANDOFF_NOTES.md`

Add new session entry:
```markdown
## Session [N] - [DATE]

**Agent:** [YOUR_NAME]
**Phase:** Phase [N]
**Duration:** [TIME_SPENT]

### Completed
- [x] [TASK_1]
- [x] [TASK_2]

### Files Changed
- `[FILE_1]`: [WHAT_CHANGED]
- `[FILE_2]`: [WHAT_CHANGED]

### Known Issues
- [ISSUE_1]: [DETAILS]

### Next Session
- Continue with [NEXT_TASK]
- Watch out for [GOTCHA]

### Notes
[Any important context for next session]
```

---

## STEP 7: OUTPUT STATUS

**Generate progress metrics:**
```bash
# Calculate task completion
PHASE_NUM=$N
TOTAL=$(grep -c "\[ \]\|\[~\]\|\[x\]" "docs/Phase $PHASE_NUM/PHASE_TASKS.md" 2>/dev/null || echo "0")
DONE=$(grep -c "\[x\]" "docs/Phase $PHASE_NUM/PHASE_TASKS.md" 2>/dev/null || echo "0")
IN_PROGRESS=$(grep -c "\[~\]" "docs/Phase $PHASE_NUM/PHASE_TASKS.md" 2>/dev/null || echo "0")
REMAINING=$((TOTAL - DONE - IN_PROGRESS))

# Calculate percentage
if [ "$TOTAL" -gt 0 ]; then
  PROGRESS=$((DONE * 100 / TOTAL))
else
  PROGRESS=0
fi

# Generate progress bar (10 blocks)
FILLED=$((PROGRESS / 10))
EMPTY=$((10 - FILLED))
PROGRESS_BAR=$(printf '█%.0s' $(seq 1 $FILLED))$(printf '░%.0s' $(seq 1 $EMPTY))

echo "📊 Phase $PHASE_NUM Progress: $PROGRESS_BAR $PROGRESS% ($DONE/$TOTAL)"
```

**Output:**
```
✅ SESSION COMPLETE

📊 Phase Progress:
Phase [N]: ████████░░ 80% (12/15 tasks complete)
- Completed: 12
- In progress: 1
- Remaining: 2

📝 This Session:
- [TASK_1] ✅ (tests passing)
- [TASK_2] ✅ (tests passing)

🧪 Testing:
- Unit tests: [PASSING/FAILING]
- Coverage: [X%]

🎯 Next:
When ready, run /4_test for functional validation, then /5_visual if UI changed

📄 Updated:
- docs/Phase [N]/PHASE_TASKS.md
- docs/HANDOFF_NOTES.md
```

---

## GUIDELINES

**Scale to the Task:**
- Simple tasks (config change, copy update, small fix) don't need the full ceremony. Implement, verify, commit, check the box.
- Complex tasks (new feature, architectural change, multi-file refactor) benefit from the full step-by-step flow.
- Use judgment on what level of rigor the task actually needs.

**Task Selection:**
- Work on logical units (don't leave things half-done)
- Follow dependencies (check "Notes" in PHASE_TASKS.md)
- Prefer completing subtasks over starting new tasks

**Implementation:**
- **Architecture:** Follow component structure from 2_ARCHITECTURE.md
- **Patterns:** Use code examples from PHASE_IMP.md
- **Styles:** Apply design system from 3_DESIGN_SYSTEM.md
  - Use defined color palette (not arbitrary colors)
  - Use typography scale (not random font sizes)
  - Use spacing system (not magic numbers)
- **Quality:** Meet standards from 4_QUALITY_ASSURANCE.md
  - Test coverage targets
  - Performance benchmarks
  - Security requirements
- **Commits:** Clear, conventional commit messages

**Documentation:**
- Update PHASE_TASKS.md as you work
- Add session to HANDOFF_NOTES.md when done
- Note any blockers or issues discovered
- Document file changes and why

**Testing:**
- **Unit tests:** Write and run for all new code
- **Tests must pass:** Before marking tasks complete
- **Coverage:** Meet targets from 4_QUALITY_ASSURANCE.md
- **Commands:** Use test commands from PHASE_IMP.md
- **Environment:** Verify in actual environment (Docker/local/cloud)
- **UI Validation:** Check matches 3_DESIGN_SYSTEM.md (if applicable)
- **No assumptions:** Always run tests, don't assume they pass

---

## COMMON PATTERNS

**Finding next task:**
```bash
# In PHASE_TASKS.md, search for:
grep -n "\[ \]" "docs/Phase $N/PHASE_TASKS.md" | head -1
```

**Checking task progress:**
```bash
# Count completed vs total
TOTAL=$(grep -c "\[ \]\|\[x\]\|\[~\]" "docs/Phase $N/PHASE_TASKS.md")
DONE=$(grep -c "\[x\]" "docs/Phase $N/PHASE_TASKS.md")
PROGRESS=$((DONE * 100 / TOTAL))

# Generate visual progress bar
FILLED=$((PROGRESS / 10))
EMPTY=$((10 - FILLED))
BAR=$(printf '█%.0s' $(seq 1 $FILLED))$(printf '░%.0s' $(seq 1 $EMPTY))

echo "Phase $N: $BAR $PROGRESS% ($DONE/$TOTAL)"
```

**Quick reference lookups:**
```bash
# Tech stack
grep -A 10 "Tech Stack:" docs/2_ARCHITECTURE.md

# Color palette (if UI work)
grep -A 20 "Color" docs/3_DESIGN_SYSTEM.md

# Test coverage targets
grep -A 5 "Coverage" docs/4_QUALITY_ASSURANCE.md

# Architecture patterns
grep -A 10 "Key Components:" docs/2_ARCHITECTURE.md
```

---

## TROUBLESHOOTING

**No phases exist:**
- Run `/2_pm` first to create a phase

**All tasks complete:**
- Celebrate.
- Run `/2_pm` to plan next phase
- Or use `/4_test` (then `/5_visual`) to verify quality

**Blocker found:**
- Note in HANDOFF_NOTES.md
- Mark task as blocked in PHASE_TASKS.md
- Move to next unblocked task

**Unclear requirements:**
- Check PHASE_PRD.md for context
- Review user stories
- Ask user for clarification

---

**Start:** STEP 0: WORKTREE SETUP, then STEP 1: LOAD PROJECT CONTEXT
