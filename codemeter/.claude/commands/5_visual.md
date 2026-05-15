# Visual - UI / Visual Regression Validation

**Usage:** `/5_visual [phase number]` or just `/5_visual` (validates latest phase or current branch)

**Purpose:** Drive the UI through critical pages and flows, capture screenshots across viewports, diff against committed baselines, and surface console errors. This is the complement to `/4_test` — that runs unit/integration/e2e correctness; this runs *visual* correctness.

**Not every step applies every time.** A backend-only change skips this whole command. A small CSS tweak only needs the affected pages re-captured. A design-system update warrants the full sweep. Use judgment.

---

## STEP 1: LOAD PROJECT CONTEXT

```bash
# Read project foundation
cat CLAUDE.md                       # Project overview & commands
cat docs/2_ARCHITECTURE.md          # Tech stack & patterns
cat docs/3_DESIGN_SYSTEM.md         # Colors, typography, spacing
cat docs/4_QUALITY_ASSURANCE.md     # Visual coverage standards (if any)
cat docs/HANDOFF_NOTES.md           # What changed recently

# Determine phase context
LATEST_PHASE=$(ls docs/Phase\ */ -d 2>/dev/null | sed 's/.*Phase //' | sed 's/\/.*//' | sort -n | tail -1)
[ -n "$LATEST_PHASE" ] && echo "Validating Phase $LATEST_PHASE"

# Load phase context if relevant
[ -n "$LATEST_PHASE" ] && cat "docs/Phase $LATEST_PHASE/PHASE_PRD.md" 2>/dev/null
```

**Understand:**
- Project type (web SPA, multi-page, mobile, native iOS, hybrid)
- Design tokens from `3_DESIGN_SYSTEM.md` (colors, typography, spacing)
- Critical flows from `PHASE_PRD.md` (login, checkout, settings, etc.)
- Recently touched components from `git log` and `HANDOFF_NOTES.md`

---

## STEP 2: PICK THE RIGHT TOOL

| Project type | Tool |
|---|---|
| Web (React, Vue, Svelte, etc.) | **Playwright MCP** (`mcp__playwright__browser_*`) |
| iOS / SwiftUI / UIKit | **Simulator + xcrun simctl screenshot** |
| Android | **adb shell screencap** |
| Desktop (Electron, Tauri) | **Playwright MCP** (Electron) or OS-level screenshot |
| CLI / API-only | **Skip this command entirely** |

```bash
# Detect project type
if [ -f apps/ios/Project.pbxproj ] || ls *.xcodeproj 2>/dev/null | head -1; then
  PROJECT_TYPE="ios"
elif [ -f package.json ] && grep -qE '"(react|vue|svelte|next|nuxt|astro)"' package.json; then
  PROJECT_TYPE="web"
elif [ -f android/build.gradle ] || ls *.apk 2>/dev/null | head -1; then
  PROJECT_TYPE="android"
else
  PROJECT_TYPE="unknown"
fi
echo "Detected: $PROJECT_TYPE"
```

---

## STEP 3: ENVIRONMENT CHECK

**Make sure the thing being captured is actually running.**

```bash
# Web — start dev server if not up (use project's start command from CLAUDE.md)
curl -sf http://localhost:3000 >/dev/null || echo "Dev server not running — start it"

# iOS — boot simulator
xcrun simctl list devices booted | grep -q Booted || \
  xcrun simctl boot "iPhone 15 Pro"

# Verify the build under test is the current branch HEAD (not a stale dev build)
git rev-parse HEAD
```

**Verify:**
- Frontend dev server / preview build is up
- Backend services it depends on are running
- Simulator is booted (mobile)
- App build is current with branch HEAD

---

## STEP 4: DEFINE THE CAPTURE PLAN

**Build the list of (route, viewport, state) tuples to capture.**

Sources:
1. `docs/Phase $N/PHASE_PRD.md` → user flows
2. `docs/3_DESIGN_SYSTEM.md` → component states (hover, focus, disabled, error)
3. `git diff origin/main...HEAD` → recently touched components
4. Project-defined critical routes (from `playwright.config` or similar)

**Output a plan like this:**

```
CAPTURE PLAN

Routes (web):
  /                       [desktop, tablet, mobile]   [logged-out]
  /login                  [desktop, mobile]           [empty, error]
  /dashboard              [desktop, tablet, mobile]   [logged-in]
  /settings/profile       [desktop, mobile]           [logged-in]

Components (Storybook, if available):
  Button                  [default, hover, disabled, loading]
  Modal                   [open, with-form, error-state]

Viewports:
  desktop  1440x900
  tablet   768x1024
  mobile   390x844 (iPhone 14 baseline)

Theme variants: light + dark (if project supports both)
```

**Keep it tight.** First pass: critical flows only. Don't capture every screen — capture what changed plus the top-3 user flows.

---

## STEP 5: CAPTURE SCREENSHOTS

### 5a. Web (Playwright MCP)

For each (route, viewport, state):

```javascript
// Resize for the viewport
mcp__playwright__browser_resize({ width: 1440, height: 900 })

// Navigate
mcp__playwright__browser_navigate({ url: "http://localhost:3000/dashboard" })

// Wait for the page to settle (network idle + critical element visible)
mcp__playwright__browser_wait_for({ text: "Welcome back" })

// Capture
mcp__playwright__browser_take_screenshot({
  filename: "tests/visual/baselines/dashboard-desktop.png",
  fullPage: true
})
```

**For component / state captures** (hover, focus, error), drive the interaction first then screenshot:

```javascript
mcp__playwright__browser_hover({ element: "Primary button", ref: "button.primary" })
mcp__playwright__browser_take_screenshot({ filename: "tests/visual/dashboard-cta-hover.png" })
```

**Hide volatile content before capturing** (dates, randomized data, animated avatars) — otherwise every run diffs. Either:
- Inject CSS that masks `.timestamp`, `.avatar`, etc.
- Stub APIs with deterministic fixtures

### 5b. iOS Simulator

```bash
# Capture currently-displayed simulator screen
xcrun simctl io booted screenshot tests/visual/baselines/dashboard-iphone15.png

# For specific device sizes, boot the right simulator first
xcrun simctl boot "iPhone SE (3rd generation)"
# ... drive the app via XCUITest or manual nav ...
xcrun simctl io booted screenshot tests/visual/baselines/dashboard-iphoneSE.png
```

**Tip:** Pair with `xcrun simctl ui` to set status bar to a known state (time 9:41, full battery) so screenshots are stable across runs:
```bash
xcrun simctl status_bar booted override --time "9:41" --batteryState charged --batteryLevel 100
```

### 5c. Android

```bash
adb shell screencap -p /sdcard/screen.png
adb pull /sdcard/screen.png tests/visual/baselines/screen-android.png
```

---

## STEP 6: DIFF AGAINST BASELINES

**Compare new captures with committed baselines.** Baselines live in `tests/visual/baselines/`. New captures go to `tests/visual/current/`.

```bash
mkdir -p tests/visual/diffs

# Use a perceptual diff tool. Install ImageMagick once: brew install imagemagick
for baseline in tests/visual/baselines/*.png; do
  name=$(basename "$baseline")
  current="tests/visual/current/$name"
  diff_out="tests/visual/diffs/$name"

  if [ ! -f "$current" ]; then
    echo "MISSING current capture: $name"
    continue
  fi

  # AE = pixel difference count; threshold of 100 px filters anti-alias noise
  pixels=$(compare -metric AE -fuzz 5% "$baseline" "$current" "$diff_out" 2>&1 | grep -oE '^[0-9]+')
  if [ "$pixels" -gt 100 ]; then
    echo "VISUAL DIFF: $name ($pixels px changed)"
  else
    rm "$diff_out"
  fi
done
```

**Alternatives if you don't want ImageMagick:**
- `playwright test --update-snapshots=missing` if the project already uses Playwright's built-in screenshot assertions
- `reg-cli` for HTML diff reports

**Three outcomes per screenshot:**
1. **Identical** — pass
2. **Diff within threshold** — pass (anti-aliasing noise)
3. **Diff above threshold** — needs review

---

## STEP 7: SCAN CONSOLE FOR RUNTIME ERRORS

**Visual correctness ≠ no errors.** A page may render fine while throwing in the background.

```javascript
// After each navigation, before moving on
const errors = mcp__playwright__browser_console_messages({})
// Filter for level === "error"
```

**Report any:**
- React/Vue/Svelte warnings (missing keys, deprecated APIs)
- Network errors (4xx/5xx that didn't surface in UI)
- Uncaught exceptions
- CSP violations
- Hydration mismatches (SSR/SSG frameworks)

---

## STEP 8: DESIGN-SYSTEM SPOT CHECK

Quick sanity audit against `docs/3_DESIGN_SYSTEM.md`:

- Colors used match the documented palette (not arbitrary hex)
- Typography uses the defined scale (no random px values for text)
- Spacing follows the spacing system (multiples of 4/8/etc.)
- Touch targets meet accessibility minimums (44x44 iOS, 48x48 Android)

This is a *spot check*, not a full audit. If the screenshots look right, the design system is probably being honored. If colors look off or spacing looks random, investigate.

---

## STEP 9: ACCESSIBILITY QUICK PASS

While the page is loaded, run a quick a11y check:

```javascript
// Web — via Playwright + axe (if installed)
mcp__playwright__browser_evaluate({
  function: "() => { /* run axe-core, return violations */ }"
})
```

**Look for:**
- Missing alt text on images
- Buttons without accessible names
- Color contrast below WCAG AA
- Form inputs without labels

Note any findings — fixing is out of scope for this command, but the user should see them.

---

## STEP 10: UPDATE BASELINES (only after user review)

If new captures look correct (intentional UI changes), promote them:

```bash
# Move approved captures to baseline
mv tests/visual/current/<approved>.png tests/visual/baselines/<approved>.png

# Commit baselines as part of the same PR that changed the UI
git add tests/visual/baselines/
git commit -m "test(visual): update baselines for [feature]

- [SCREEN_1]: [WHAT_CHANGED]
- [SCREEN_2]: [WHAT_CHANGED]"
```

**Never auto-promote.** Visual baselines are reviewer artifacts. The /5_visual run surfaces diffs; the human decides if each is intentional. If running in CI, fail on any unreviewed diff.

---

## STEP 11: OUTPUT VISUAL REPORT

```
VISUAL TEST REPORT

Phase: [N]
Date: [DATE]
Branch: [BRANCH]

────────────────────────────────────────

CAPTURES: [X] screenshots across [Y] routes × [Z] viewports

DIFFS:
  Identical:         [X]
  Within threshold:  [Y]
  Above threshold:   [Z]  ← review

  Above-threshold diffs:
    - dashboard-mobile.png        (1247 px changed)  → tests/visual/diffs/dashboard-mobile.png
    - settings-desktop-dark.png   (893 px changed)   → tests/visual/diffs/settings-desktop-dark.png

CONSOLE ERRORS: [N] across all pages
  - /dashboard:  "Warning: Each child in a list should have a unique key prop"
  - /login:      (none)

DESIGN-SYSTEM SPOT CHECK:
  Colors:      [PASS / FAIL — list violations]
  Typography:  [PASS / FAIL]
  Spacing:     [PASS / FAIL]
  Touch tgts:  [PASS / FAIL]

ACCESSIBILITY:
  [N] WCAG-AA violations
  - [VIOLATION_1]
  - [VIOLATION_2]

────────────────────────────────────────

OVERALL: [✅ NO ACTION / 🟡 DIFFS TO REVIEW / ❌ ISSUES FOUND]

Next:
  - Review diffs in tests/visual/diffs/
  - If intentional → run STEP 10 to promote baselines
  - If unintentional → revisit the change that caused them
  - Fix any console errors and a11y violations before /6_doc
```

---

## GUIDELINES

**Scale to context:**
- Backend-only change → skip /5_visual entirely
- Single-component tweak → re-capture only the affected screen(s) and any page that embeds it
- Design-system change → full sweep across all routes + both themes
- Pre-release → full sweep + accessibility pass

**Determinism is everything:**
- Hide timestamps, randomized avatars, animated elements before capture
- Stub APIs with fixed fixtures
- Pin viewport sizes; pin fonts (avoid network-loaded webfonts during capture)
- iOS: lock the status bar with `xcrun simctl status_bar`

**Baseline hygiene:**
- Baselines live in `tests/visual/baselines/`, committed to the repo
- One baseline per (route, viewport, theme, state) tuple
- When promoting, commit the baseline change in the *same PR* as the UI change so reviewers can see both
- Delete baselines for removed pages — orphan baselines cause confusion

**Don't:**
- Don't auto-update baselines in CI
- Don't capture against a stale build
- Don't ignore console errors just because the page rendered
- Don't run /5_visual on a feature branch without dev server pointed at that branch

**Do:**
- Capture both happy path and 1-2 error / empty / loading states per critical screen
- Run light + dark theme if the project supports both
- Run mobile + desktop minimum; tablet if the design diverges meaningfully
- For mobile-native apps, capture at least one small device (iPhone SE) and one large (Pro Max) — layout bugs hide in the extremes

---

## TROUBLESHOOTING

**Flaky diffs every run:**
- Almost always a timestamp, randomized content, or webfont load timing.
- Mask them with CSS or stub the data.

**Playwright can't find an element:**
- Use `mcp__playwright__browser_snapshot()` to dump the a11y tree and find a stable ref.
- Avoid CSS-selector targeting if the design system uses generated class names.

**iOS screenshots are blank:**
- The simulator might not have rendered yet. Add a small wait after navigation.
- Also confirm `xcrun simctl io booted screenshot` — the `booted` keyword targets the currently-booted simulator; if none is booted, the command silently writes a blank file.

**Diff threshold tuning:**
- Start with `-fuzz 5%` and threshold 100 px. If anti-aliasing on text causes false positives, raise fuzz to 8% or threshold to 500.
- If you start *missing* real diffs, lower both.

**Webfonts cause flicker:**
- Self-host fonts or stub them with system fonts for visual runs. Network-loaded fonts cause FOUT/FOIT differences across runs.

---

## DIFFERENCES FROM /4_test

| Aspect | /4_test | /5_visual |
|--------|---------|-----------|
| **Focus** | Behavior correctness | Visual / UI correctness |
| **Asserts** | Function returns, status codes, DOM presence | Pixel diff, layout, theme, design system |
| **Tools** | Unit framework, integration harness, e2e | Playwright MCP, simctl, image diff |
| **When to skip** | Never — every change needs functional test | Backend-only / non-UI changes |
| **Baselines** | Code (test assertions) | Image files in tests/visual/baselines/ |
| **Reviewer involvement** | Pass/fail is automatic | Diffs need human eyes |

Run **both** before merging UI work.

---

**Next:** If visual checks pass, run `/6_doc` to update documentation. If diffs need review, surface them to the user before promoting baselines.

**Start:** STEP 1: LOAD PROJECT CONTEXT
