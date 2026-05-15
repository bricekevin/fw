# Test - Comprehensive Quality Validation

**Usage:** `/4_test [phase number]` or just `/4_test` (validates latest phase)

**Scope:** Functional / unit / integration / e2e correctness. For pixel-level visual regression and UI testing, use `/5_visual` after this command.

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

# Find latest phase
LATEST_PHASE=$(ls docs/Phase\ */ -d 2>/dev/null | sed 's/.*Phase //' | sed 's/\/.*//' | sort -n | tail -1)

if [ -z "$LATEST_PHASE" ]; then
  echo "Error: No phases found. Run /2_pm first."
  exit 1
fi

echo "Validating Phase $LATEST_PHASE"

# Load phase context
cat "docs/Phase $LATEST_PHASE/PHASE_PRD.md"    # What to build
cat "docs/Phase $LATEST_PHASE/PHASE_TASKS.md"  # Task checklist
cat "docs/Phase $LATEST_PHASE/PHASE_IMP.md"    # How to build
```

**Understand:**
- Overall project quality targets from `4_QUALITY_ASSURANCE.md`
- Specific test commands from `PHASE_IMP.md`
- Critical user flows from `PHASE_PRD.md`

---

## STEP 2: ENVIRONMENT CHECK

```bash
# Check if services are running (from CLAUDE.md)
[CHECK_SERVICES_COMMAND]

# If not running, start them
[START_SERVICES_COMMAND]
```

**Verify:**
- All necessary services are up and healthy
- Database is accessible
- Frontend is serving (if applicable)

---

## STEP 3: RUN UNIT TESTS

```bash
# Unit test command from PHASE_IMP.md or CLAUDE.md
[UNIT_TEST_COMMAND]

# Examples:
# npm test
# pytest
# cargo test
# go test ./...
```

**Verify:**
- ✅ All unit tests pass
- ✅ Meet coverage targets from 4_QUALITY_ASSURANCE.md
- ✅ No new failing tests

**Output:**
```
🧪 UNIT TESTS

Status: [PASS/FAIL]
Coverage: [X%] (target: [Y%])
Tests: [PASSED] passed, [FAILED] failed
```

---

## STEP 4: RUN INTEGRATION TESTS

```bash
# Integration test command from project
[INTEGRATION_TEST_COMMAND]
```

**Verify:**
- ✅ API endpoints respond correctly
- ✅ Services communicate properly
- ✅ Database operations work

**Output:**
```
🔗 INTEGRATION TESTS

Status: [PASS/FAIL]
Endpoints: [X] tested
Services: [Y] verified
```

---

## STEP 5: RUN E2E TESTS

**For frontend/web apps:**
Use Playwright MCP for browser automation

```bash
# Playwright tests (if applicable)
[E2E_TEST_COMMAND]
```

**Available via MCP:**
- `mcp_playwright_navigate` - Navigate to pages
- `mcp_playwright_click` - Interact with UI
- `mcp_playwright_screenshot` - Visual verification
- `mcp_playwright_console_logs` - Check for errors
- `mcp_playwright_get_visible_text` - Validate content

**Verify:**
- ✅ Critical user flows work end-to-end
- ✅ No console errors
- ✅ UI elements present and functional

**Output:**
```
🌐 E2E TESTS

Status: [PASS/FAIL]
Scenarios: [X] tested
Screenshots: [Y] captured
Console errors: [NONE/LIST]
```

---

## STEP 6: CHECK CODE QUALITY

```bash
# Linting
[LINT_COMMAND]

# Type checking (if TypeScript/typed language)
[TYPE_CHECK_COMMAND]

# Security scan (if configured)
[SECURITY_SCAN_COMMAND]
```

**Verify:**
- ✅ No linting errors
- ✅ No type errors
- ✅ No security vulnerabilities

**Output:**
```
📋 CODE QUALITY

Linting: [PASS/FAIL]
Type safety: [PASS/FAIL]
Security: [PASS/FAIL] ([X] issues found)
```

---

## STEP 7: VALIDATE LOGS

**For backend/services:**
Check application logs for errors

```bash
# Docker logs (if applicable)
docker compose logs --tail=100

# Or application logs
[LOG_CHECK_COMMAND]
```

**MCP available for log analysis:**
- Use `mcp_playwright_console_logs` for browser console
- Check server logs manually or via grep

**Look for:**
- ❌ Error messages
- ❌ Warning messages
- ❌ Stack traces
- ✅ Successful operations

---

## STEP 8: PERFORMANCE CHECK

**If performance targets in 4_QUALITY_ASSURANCE.md:**

```bash
# Load test (if configured)
[PERFORMANCE_TEST_COMMAND]

# Benchmarks
[BENCHMARK_COMMAND]
```

**Verify:**
- ✅ Response times meet targets
- ✅ Resource usage acceptable
- ✅ No performance regressions

---

## STEP 9: OUTPUT TEST REPORT

```
✅ COMPREHENSIVE TEST REPORT

Phase: [N]
Date: [DATE]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

🧪 UNIT TESTS: [✅ PASS / ❌ FAIL]
   Tests: [X] passed, [Y] failed
   Coverage: [Z%] (target: [TARGET%])

🔗 INTEGRATION TESTS: [✅ PASS / ❌ FAIL]
   Endpoints: [X] tested
   Services: [Y] verified

🌐 E2E TESTS: [✅ PASS / ❌ FAIL]
   Scenarios: [X] passed
   Console errors: [NONE/COUNT]

📋 CODE QUALITY: [✅ PASS / ❌ FAIL]
   Linting: [✅/❌]
   Types: [✅/❌]
   Security: [✅/❌]

⚡ PERFORMANCE: [✅ PASS / ❌ FAIL]
   Response time: [Xms] (target: [Yms])
   Load: [Z] concurrent users

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

OVERALL: [✅ READY / ❌ ISSUES FOUND]

Issues Found:
[LIST_ISSUES_IF_ANY]

Next Actions:
[FIX_ITEMS_OR_PROCEED_TO_DOC]
```

---

## GUIDELINES

**Scale to Context:**
- Not every step applies every time. Skip steps that don't exist for the project (e.g., no E2E tests? skip Step 5. No performance targets? skip Step 8).
- For small changes, unit tests + build check may be sufficient.
- For phase completion or pre-release, run the full suite.
- Use judgment -- the goal is confidence, not ceremony.

**Test Execution:**
- Run in actual environment (not mocked unless necessary)
- Use test commands from PHASE_IMP.md or CLAUDE.md
- Do NOT create new test scripts outside testing suites
- Add tests to existing test suites if missing

**Playwright MCP Usage:**
- For web apps, use Playwright MCP tools
- Check console logs for errors
- Take screenshots for visual verification
- Validate user flows work as expected

**Quality Standards:**
- Must meet coverage targets from 4_QUALITY_ASSURANCE.md
- All tests must pass before marking phase complete
- Performance must meet benchmarks
- No security vulnerabilities

**Logging:**
- Check application logs for errors
- Use MCP for browser console logs
- Validate no unexpected errors

**Failure Handling:**
- If tests fail → Document in HANDOFF_NOTES.md
- List specific failing tests
- Note error messages
- Do NOT mark phase complete if tests fail

---

## PLAYWRIGHT MCP TOOLS

**Available for frontend testing:**

```javascript
// Navigate
mcp_playwright_navigate({ url: "http://localhost:3000" })

// Interact
mcp_playwright_click({ selector: ".login-button" })
mcp_playwright_fill({ selector: "#email", value: "test@example.com" })

// Validate
mcp_playwright_get_visible_text()
mcp_playwright_screenshot({ name: "login-page" })

// Check logs
mcp_playwright_console_logs({ type: "error" })
```

**Use for:**
- Critical user flow validation
- Visual regression checks
- Console error detection
- UI element verification

---

## TROUBLESHOOTING

**Tests won't run:**
- Check test commands in PHASE_IMP.md or CLAUDE.md
- Verify environment is running (Docker/services)
- Check dependencies are installed

**Low coverage:**
- Identify untested code
- Add unit tests to existing test suite
- Do NOT create separate test scripts

**E2E tests failing:**
- Use Playwright MCP to debug interactively
- Take screenshots to see UI state
- Check console logs for errors
- Verify selectors are correct

**Performance issues:**
- Check targets in 4_QUALITY_ASSURANCE.md
- Profile slow operations
- Document findings in HANDOFF_NOTES.md

---

**Next:** If functional tests pass and the change touched UI, run `/5_visual` next. Otherwise run `/6_doc` to update documentation.

**Start:** STEP 1: LOAD TEST CONTEXT

