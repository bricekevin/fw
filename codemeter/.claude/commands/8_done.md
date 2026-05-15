# Done - Session Completion & Handoff

**Usage:** `/8_done` (wraps up session, ensures no loose ends before context clear)

**Purpose:** Run this before clearing context to verify all work is saved, documented, and ready for the next session.

**Not every step will apply every time.** A quick planning session may only need git commit + handoff notes. A full implementation session warrants the complete checklist. Use judgment.

---

## STEP 1: LOAD SESSION CONTEXT

```bash
# Identify current work
cat docs/HANDOFF_NOTES.md                  # What was being worked on
ls docs/Phase\ */ -d 2>/dev/null           # List all phases

# Find latest phase
LATEST_PHASE=$(ls docs/Phase\ */ -d 2>/dev/null | sed 's/.*Phase //' | sed 's/\/.*//' | sort -n | tail -1)
echo "Latest phase: $LATEST_PHASE"
```

**Understand:**
- What task was being worked on
- Which phase is active
- What the expected deliverables were

---

## STEP 2: CHECK TASK COMPLETION

```bash
# Read the active phase task list
cat "docs/Phase $LATEST_PHASE/PHASE_TASKS.md"

# Count task status
TOTAL=$(grep -c "\[ \]\|\[~\]\|\[x\]" "docs/Phase $LATEST_PHASE/PHASE_TASKS.md" 2>/dev/null || echo "0")
DONE=$(grep -c "\[x\]" "docs/Phase $LATEST_PHASE/PHASE_TASKS.md" 2>/dev/null || echo "0")
IN_PROGRESS=$(grep -c "\[~\]" "docs/Phase $LATEST_PHASE/PHASE_TASKS.md" 2>/dev/null || echo "0")
PENDING=$((TOTAL - DONE - IN_PROGRESS))

echo "Complete: $DONE | In Progress: $IN_PROGRESS | Pending: $PENDING | Total: $TOTAL"
```

**Check:**
- Are there any `[~]` (in-progress) tasks that should be `[x]` (complete)?
- Were all intended subtasks checked off?
- If work is incomplete, is the current state accurately reflected?

**If tasks were completed this session:**
- Update `[ ]` or `[~]` to `[x]` for finished work
- Leave genuinely incomplete work as `[ ]` or `[~]`

---

## STEP 3: CHECK DOCUMENTATION UPDATES

**Verify these docs reflect current state:**

### PHASE_TASKS.md
- Are checkbox statuses accurate?
- Do they match what was actually implemented?

### HANDOFF_NOTES.md
- Is there a session entry for this work session?
- Does it include: what was done, files changed, known issues, next steps?

**If handoff notes need updating, add:**
```markdown
## Session [N] - [DATE]

**Agent:** [NAME]
**Phase:** Phase [N]
**Task:** [TASK_WORKED_ON]

### Completed
- [WHAT_WAS_DONE]

### Files Changed
- `[FILE]`: [CHANGES]

### Known Issues
| Issue | Severity | Notes |
|-------|----------|-------|
| [ISSUE] | [High/Med/Low] | [DETAILS] |

### Next Session
- Next task: [NEXT_TASK]
- Watch out for: [GOTCHAS]
- Blockers: [BLOCKERS_IF_ANY]
```

---

## STEP 4: CHECK GIT STATUS

```bash
# Uncommitted changes
git status

# Recent commits this session
git log --oneline -10
```

**Evaluate:**
- **CLEAN:** All changes committed - good to go
- **UNCOMMITTED:** Files need committing before clearing context

**If uncommitted changes exist:**
```bash
git add [FILES]
git commit -m "[type](scope): [description]

- [change 1]
- [change 2]"
```

**Commit types:** feat, fix, refactor, test, docs, chore

---

## STEP 5: CHECK GIT SYNC

```bash
# Fetch latest remote state
git fetch origin

# Get current branch
CURRENT_BRANCH=$(git branch --show-current)
echo "Current branch: $CURRENT_BRANCH"

# Check remote tracking
git branch -vv | grep "$CURRENT_BRANCH"

# Check if local is ahead of remote
git log --oneline origin/$CURRENT_BRANCH..$CURRENT_BRANCH 2>/dev/null | head -5

# Check if remote is ahead of local
git log --oneline $CURRENT_BRANCH..origin/$CURRENT_BRANCH 2>/dev/null | head -5
```

**Evaluate:**
- **IN SYNC:** Local and remote match
- **LOCAL AHEAD:** Unpushed commits - offer to push
- **REMOTE AHEAD:** Need to pull before next session
- **DIVERGED:** Branches have diverged - note for next session

**If local is ahead and should be pushed:**
```bash
git push origin $CURRENT_BRANCH
```

---

## STEP 5.5: WORKTREE CLEANUP

If this session ran in a sibling worktree (see `CLAUDE.md` → "Parallel Sessions & Worktrees"), tidy up so the directory tree stays clean.

```bash
CURRENT_DIR=$(git rev-parse --show-toplevel)
MAIN_WORKTREE=$(git worktree list | head -n 1 | awk '{print $1}')
CURRENT_BRANCH=$(git branch --show-current)

if [ "$CURRENT_DIR" = "$MAIN_WORKTREE" ]; then
  echo "Session ran in main checkout — no worktree to clean up."
else
  # Is this branch already merged into the default branch?
  DEFAULT_BRANCH=$(git -C "$MAIN_WORKTREE" symbolic-ref refs/remotes/origin/HEAD 2>/dev/null | sed 's@^refs/remotes/origin/@@' || echo "main")
  git fetch origin "$DEFAULT_BRANCH" --quiet
  MERGED=$(git merge-base --is-ancestor "$CURRENT_BRANCH" "origin/$DEFAULT_BRANCH" && echo "yes" || echo "no")

  if [ "$MERGED" = "yes" ]; then
    echo "Branch $CURRENT_BRANCH is merged into origin/$DEFAULT_BRANCH."
    echo "Cleaning up worktree at $CURRENT_DIR ..."
    # Step out before removing
    cd "$MAIN_WORKTREE"
    git worktree remove "$CURRENT_DIR"
    git branch -d "$CURRENT_BRANCH" 2>/dev/null || true
    git worktree prune
    echo "Worktree removed."
  else
    echo "Branch $CURRENT_BRANCH not yet merged — leaving worktree at $CURRENT_DIR for next session."
  fi
fi
```

**When to skip cleanup:**
- Worktree has uncommitted work the user hasn't shipped yet.
- User explicitly wants to keep the worktree around (long-running feature branch).
- The branch is still open as a PR awaiting review.

In any of these cases, leave the worktree and note its path in `HANDOFF_NOTES.md` so the next session can `cd` straight back in.

---

## STEP 6: QUICK BUILD & TEST SANITY CHECK

**This is NOT a full test run** -- use `/4_test` for comprehensive validation. This is a quick sanity check to catch obvious breakage before clearing context.

**If `/4_test` was already run this session and passed, skip to Step 7.**

Otherwise, run a quick check appropriate to the project:

```bash
# Quick build check (pick what fits the project)
npm run typecheck 2>/dev/null || npx tsc --noEmit 2>/dev/null  # TypeScript
# mypy . 2>/dev/null       # Python
# go build ./... 2>/dev/null  # Go
# cargo check 2>/dev/null    # Rust

# Quick test run
npm test 2>/dev/null         # Node
# pytest 2>/dev/null         # Python
# go test ./... 2>/dev/null  # Go
```

**Report:**
- **PASSING:** No obvious issues
- **FAILING:** Fix if quick, otherwise document in HANDOFF_NOTES.md for next session
- **SKIPPED:** Already validated via `/4_test` this session

---

## STEP 7: OUTPUT COMPLETION REPORT

**Generate the session wrap-up:**

```
## Session Completion Status

| Check | Status | Notes |
|-------|--------|-------|
| Task Completion | [STATUS] | [DETAILS] |
| Task List Updated | [STATUS] | [DETAILS] |
| Handoff Notes | [STATUS] | [DETAILS] |
| Git Status | [STATUS] | [DETAILS] |
| Git Sync | [STATUS] | [SYNC_STATE] |
| Build/Tests | [STATUS] | [DETAILS] |
```

**Status indicators:**
- Use checkmark for passing/complete
- Use X for failing/incomplete
- Use N/A where not applicable

```
## Loose Ends

[LIST_ANY_UNRESOLVED_ITEMS]
- [ ] [ITEM_1]
- [ ] [ITEM_2]
(or "None - clean handoff")

## Next Step Recommendation

Based on the handoff notes and task checklist, the next action is:

**Option A**: Continue current phase
- Next task: [TASK_X.X: DESCRIPTION]
- Command: /3_dev [PHASE_NUMBER]

**Option B**: Phase complete - validate
- Run /4_test for functional check
- Run /5_visual if UI changed
- Then /6_doc to update documentation

**Option C**: Phase complete - plan next
- Run /2_pm to plan the next phase

**Option D**: Ad-hoc work needed
- Run /7_go [DESCRIPTION]

**Option E**: Ready to ship a release
- Run /9_deploy

---
Ready to clear context. Run the recommended command above to continue.
```

---

## GUIDELINES

**Before Clearing Context:**
- Never leave uncommitted code changes
- Never leave `[~]` tasks that are actually complete
- Always update HANDOFF_NOTES.md so the next session has context
- Push to remote if working on a shared branch
- Document any blockers or gotchas for next session

**What Gets Lost on Context Clear:**
- Current conversation and reasoning
- Uncommitted file changes (if not saved)
- In-flight decisions not written down
- Knowledge of what was tried and didn't work

**What Survives Context Clear:**
- Git commits and history
- HANDOFF_NOTES.md session entries
- PHASE_TASKS.md checkbox status
- All documentation updates
- Pushed branches

**Priority Order (if short on time):**
1. Commit all code changes (most critical)
2. Update PHASE_TASKS.md checkboxes
3. Add HANDOFF_NOTES.md entry
4. Push to remote
5. Run build/test checks

---

## TROUBLESHOOTING

**No phases exist:**
- This is a new project - just check git status and handoff notes
- Recommend `/1_start` or `/2_pm` for next session

**Can't determine what was worked on:**
- Check `git diff` and `git log` for recent changes
- Review file modification times
- Ask user what they were working on

**Tests failing:**
- Document failures in HANDOFF_NOTES.md
- Note as a blocker for next session
- Don't leave without at least documenting the state

**Merge conflicts or git issues:**
- Document the state in HANDOFF_NOTES.md
- Note exact branch state and what needs resolution
- Don't force-push or make destructive changes

**Nothing to wrap up:**
- Session may have been exploratory or planning-only
- Still update HANDOFF_NOTES.md with findings
- Note what was explored and conclusions reached

---

## DIFFERENCES FROM OTHER COMMANDS

| Aspect | /8_done | /3_dev | /6_doc |
|--------|---------|--------|--------|
| **Purpose** | Wrap up session | Implement tasks | Update docs |
| **When** | Before context clear | During development | After implementation |
| **Focus** | No loose ends + worktree cleanup | Code completion | Doc accuracy |
| **Output** | Status report | Progress bar | Doc report |
| **Commits** | Ensures all committed | Commits per task | Commits doc changes |

**When to use /8_done:**
- Before clearing context
- End of work session
- Switching to different project
- Before long break from project
- Handing off to another developer

**When NOT to use /8_done:**
- Mid-task (finish or pause task first)
- Just started session (nothing to wrap up)
- Use /6_doc instead if you need doc updates only

---

**Start:** STEP 1: LOAD SESSION CONTEXT
