# Doc - Update Project Documentation

**Usage:** `/6_doc` (reviews and updates all project docs)

---

## STEP 1: REVIEW RECENT CHANGES

```bash
# Check git history
git log --oneline -20

# Check what was done recently
cat docs/HANDOFF_NOTES.md | tail -100
```

**Understand:**
- What features were added
- What files were changed
- What needs documentation updates

---

## STEP 2: SCAN ALL DOCS

**Read all project documentation:**

```bash
# Foundation docs
cat CLAUDE.md
cat README.md
cat docs/1_PROJECT_OVERVIEW.md
cat docs/2_ARCHITECTURE.md
cat docs/3_DESIGN_SYSTEM.md
cat docs/4_QUALITY_ASSURANCE.md
cat docs/HANDOFF_NOTES.md

# Phase docs
for phase in docs/Phase\ */; do
  cat "$phase/PHASE_PRD.md"
  cat "$phase/PHASE_TASKS.md"
  cat "$phase/PHASE_IMP.md"
done
```

**Look for:**
- ❌ Outdated information
- ❌ Missing new features
- ❌ Incorrect code examples
- ❌ Broken references
- ❌ Inconsistencies

---

## STEP 3: IDENTIFY UPDATES NEEDED

**For each doc, check if updates needed:**

### CLAUDE.md
- ✅ Project overview accurate?
- ✅ Tech stack current?
- ✅ Key commands work?
- ✅ Architecture flow matches reality?

### README.md
- ✅ Quick start works?
- ✅ Installation steps correct?
- ✅ Features list complete?
- ✅ Usage examples accurate?

### 1_PROJECT_OVERVIEW.md
- ✅ Goals still accurate?
- ✅ Features list current?
- ✅ Scope reflects reality?
- ✅ Timeline updated?

### 2_ARCHITECTURE.md
- ✅ Components list complete?
- ✅ Data flow accurate?
- ✅ Database schema current?
- ✅ API endpoints documented?
- ✅ Tech stack matches code?

### 3_DESIGN_SYSTEM.md
- ✅ Colors match implementation?
- ✅ Typography accurate?
- ✅ Components documented?
- ✅ New UI patterns added?

### 4_QUALITY_ASSURANCE.md
- ✅ Test coverage targets met?
- ✅ Testing strategy accurate?
- ✅ New test types added?
- ✅ CI/CD pipeline current?

### HANDOFF_NOTES.md
- ✅ Latest session documented?
- ✅ Issues tracked?
- ✅ Follow-ups noted?

### Phase Docs
**For each Phase N:**

#### PHASE_PRD.md
- ✅ Requirements still accurate?
- ✅ User stories reflect implementation?
- ✅ Scope matches what was built?

#### PHASE_TASKS.md
- ✅ Task statuses current?
- ✅ All checkboxes updated?
- ✅ Blockers resolved/noted?

#### PHASE_IMP.md
- ✅ Code examples accurate?
- ✅ Setup steps work?
- ✅ Deployment process current?
- ✅ New APIs/components documented?

---

## STEP 4: UPDATE DOCS (DO NOT CREATE NEW)

**For each doc needing updates:**

```
📝 Updating: [DOC_NAME]

Changes needed:
- [CHANGE_1]
- [CHANGE_2]
- [CHANGE_3]

[APPLY UPDATES TO EXISTING DOCUMENT]

✅ Updated: [DOC_NAME]
```

**Update guidelines:**
- ✅ Update existing docs only
- ❌ Do NOT create new markdown files
- ✅ Keep structure intact
- ✅ Add missing information
- ✅ Correct inaccuracies
- ✅ Update code examples
- ✅ Fix broken references

---

## STEP 5: VERIFY CONSISTENCY

**Check cross-doc consistency:**

```bash
# Tech stack should match everywhere
grep -i "tech stack" docs/*.md

# Architecture should be consistent
grep -i "components" docs/2_ARCHITECTURE.md docs/Phase*/PHASE_IMP.md

# Features should align
grep -i "features" docs/1_PROJECT_OVERVIEW.md README.md
```

**Ensure:**
- Tech stack mentioned consistently
- Architecture diagrams align
- Feature lists match
- API endpoints documented everywhere they're referenced

---

## STEP 6: UPDATE CODE EXAMPLES

**If code examples in docs are outdated:**

```
📝 Updating code examples in [DOC_NAME]

Old example:
```[language]
[OLD_CODE]
```

New example:
```[language]
[NEW_CODE]
```

Reason: [WHY_UPDATED]
```

**Focus on:**
- 2_ARCHITECTURE.md - Component examples
- PHASE_IMP.md - Implementation code
- CLAUDE.md - Command examples
- README.md - Usage examples

---

## STEP 7: COMMIT DOCUMENTATION UPDATES

```bash
git add docs/
git add CLAUDE.md README.md
git commit -m "docs: update documentation after Phase [N] implementation

- Updated [DOC_1]: [CHANGES]
- Updated [DOC_2]: [CHANGES]
- Fixed code examples in [DOC_3]
"
```

---

## STEP 8: OUTPUT DOCUMENTATION REPORT

```
✅ DOCUMENTATION UPDATED

📊 Summary:
- Docs reviewed: [X]
- Docs updated: [Y]
- Issues fixed: [Z]

📝 Updated Files:
- CLAUDE.md: [CHANGES]
- README.md: [CHANGES]
- docs/2_ARCHITECTURE.md: [CHANGES]
- docs/Phase [N]/PHASE_IMP.md: [CHANGES]

🔍 Consistency Checks:
- Tech stack: ✅ Consistent
- Architecture: ✅ Aligned
- Features: ✅ Updated
- Code examples: ✅ Current

✅ All documentation is now current and accurate
```

---

## GUIDELINES

**Scale to What Changed:**
- Small change (bug fix, minor tweak)? Check HANDOFF_NOTES.md and PHASE_TASKS.md, update if needed, done.
- New feature or significant change? Review the relevant docs (architecture, design system, etc.) and update what's stale.
- Phase completion? Full doc sweep makes sense.
- Don't re-read and re-validate every doc for a one-line fix.

**Update Philosophy:**
- Update existing docs, never create new markdown files
- Keep doc structure from templates intact
- Add missing information to appropriate sections
- Fix inaccuracies and outdated info
- Update code examples to match current implementation

**What to Update:**
- New features in PROJECT_OVERVIEW.md
- New components in ARCHITECTURE.md
- New UI patterns in DESIGN_SYSTEM.md
- Test results in QUALITY_ASSURANCE.md
- Latest session in HANDOFF_NOTES.md
- Completed tasks in PHASE_TASKS.md
- Implementation details in PHASE_IMP.md

**Code Examples:**
- Must be actual working code
- Match current tech stack
- Include necessary imports
- Be concise but complete
- Reference real file paths

**Consistency:**
- Tech stack mentioned same way everywhere
- Architecture diagrams align across docs
- Feature lists match between docs
- API endpoints documented consistently

---

## COMMON UPDATES

**After Phase implementation:**
```
2_ARCHITECTURE.md:
- Add new components to component list
- Update data flow diagram
- Add new API endpoints
- Document new database tables

PHASE_IMP.md:
- Add actual implementation code examples
- Update setup steps
- Document deployment process
- Add troubleshooting tips

README.md:
- Update feature list
- Add usage examples
- Update installation if changed
```

**After testing:**
```
4_QUALITY_ASSURANCE.md:
- Update test coverage percentages
- Add new test types if introduced
- Document CI/CD improvements
- Note any quality issues resolved
```

**After new features:**
```
1_PROJECT_OVERVIEW.md:
- Add features to feature list
- Update success metrics
- Note scope changes

CLAUDE.md:
- Add new commands
- Update key patterns
- Document new conventions
```

---

## TROUBLESHOOTING

**Don't know what to update:**
- Review git log for changes
- Check HANDOFF_NOTES.md for recent work
- Look at Phase N tasks that were completed
- Compare docs to actual codebase

**Docs seem accurate:**
- Still verify code examples work
- Check cross-doc consistency
- Update dates/timestamps
- Ensure completeness

**Code examples broken:**
- Test them in actual project
- Update to working versions
- Add comments for clarity
- Reference correct file paths

---

**Workflow:**
```
/3_dev   → Implement tasks
/3_dev   → More tasks
/4_test  → Validate functional correctness
/5_visual → Validate UI (if UI changed)
/6_doc   → Update documentation ← YOU ARE HERE
/3_dev   → Continue with next phase
```

**Start:** STEP 1: REVIEW RECENT CHANGES

