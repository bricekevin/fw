// ui.ino — M5.Lcd screen drawing.
//
// Phase 1 screens are text-only (no PNG assets — those land in Phase 3).
// Colors and layout padding come from config.h. Panel is 320x240.

// --- Helpers ----------------------------------------------------------------

static void ui_header(const char *title) {
  M5.Lcd.fillRect(0, 0, 320, STATUSBAR_H, COLOR_SURFACE);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.setTextColor(COLOR_PRIMARY);
  M5.Lcd.drawString(title, PAD_EDGE, STATUSBAR_H / 2 + 1);
}

static String ui_format_uptime(unsigned long secs) {
  unsigned long h = secs / 3600;
  unsigned long m = (secs % 3600) / 60;
  unsigned long s = secs % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

static void ui_status_row(int x, int y, const char *label,
                          const String &value) {
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString(label, x, y);
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString(value, x + 110, y);
}

// --- Screens ----------------------------------------------------------------

void ui_show_splash() {
  M5.Lcd.fillScreen(COLOR_BG);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSSB12);
  M5.Lcd.setTextColor(COLOR_PRIMARY);
  M5.Lcd.drawString("M5Clawd", 160, 104);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString("v" FW_VERSION, 160, 140);
}

void ui_show_connecting() {
  M5.Lcd.fillScreen(COLOR_BG);
  ui_header("CONNECTING");
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSS12);
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString("Connecting to WiFi...", 160, 124);
}

void ui_show_wifi_error() {
  M5.Lcd.fillScreen(COLOR_BG);
  ui_header("WIFI ERROR");
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSS12);
  M5.Lcd.setTextColor(COLOR_ERROR);
  M5.Lcd.drawString("WiFi connect failed", 160, 110);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString("Hold button C 5s to reconfigure", 160, 144);
}

void ui_show_status() {
  M5.Lcd.fillScreen(COLOR_BG);
  ui_header("STATUS");
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setFreeFont(FSS9);

  bool connected = (WiFi.status() == WL_CONNECTED);
  int  x   = PAD_EDGE;
  int  y   = STATUSBAR_H + PAD_SECTION;
  const int row = 26;

  ui_status_row(x, y, "Firmware", String("v") + FW_VERSION);
  y += row;
  ui_status_row(x, y, "WiFi",
                connected ? WiFi.SSID() : String("disconnected"));
  y += row;
  ui_status_row(x, y, "Signal",
                connected ? String(WiFi.RSSI()) + " dBm" : String("--"));
  y += row;
  ui_status_row(x, y, "IP",
                connected ? WiFi.localIP().toString() : String("--"));
  y += row;
  ui_status_row(x, y, "Free heap", String(ESP.getFreeHeap()) + " B");
  y += row;
  ui_status_row(x, y, "Uptime", ui_format_uptime(millis() / 1000));
}

// --- Usage screen ----------------------------------------------------------
// Layout (320x240): an 18px status bar, then two stacked cards (SESSION /
// WEEKLY). See docs/3_DESIGN_SYSTEM.md section "2. Usage". ui_show_usage()
// paints the whole screen; ui_update_usage() redraws only the fields whose
// value changed, so a per-poll refresh never flickers titles or background.
//
// The y-offsets below are hand-tuned for the panel; adjust against a photo
// (the embedded /5_visual step) if a card looks cramped.

#define USG_CARD1_TOP  (STATUSBAR_H)   // 18
#define USG_CARD2_TOP  129
#define USG_TITLE_DY   6               // title y within a card
#define USG_VAL_TOP    24              // value area: cardTop + this ...
#define USG_VAL_H      60              // ... this tall (cleared on redraw)
#define USG_RESET_DY   88              // reset-countdown text y within a card

// Per-field cache so ui_update_usage() can skip unchanged fields.
static int      s_usg_session = -1;
static int      s_usg_weekly  = -1;
static uint32_t s_usg_sreset  = 0xFFFFFFFFUL;
static uint32_t s_usg_wreset  = 0xFFFFFFFFUL;
static int      s_usg_status  = -1;
static String   s_usg_updated = "";

// Map a poll status to a status-bar badge (text + color). Text plus color,
// never color alone — see the design system's accessibility note.
static void usg_badge(UsageData::Status st, const char **txt, uint16_t *col) {
  switch (st) {
    case UsageData::OK:              *txt = "poll ok";     *col = COLOR_SUCCESS; break;
    case UsageData::WIFI_DOWN:       *txt = "wifi down";   *col = COLOR_ERROR;   break;
    case UsageData::API_UNREACHABLE: *txt = "api error";   *col = COLOR_ERROR;   break;
    case UsageData::AUTH_FAILED:     *txt = "auth failed"; *col = COLOR_ERROR;   break;
    case UsageData::RATE_LIMITED:    *txt = "rate limited";*col = COLOR_WARNING; break;
    case UsageData::REAUTH_REQUIRED: *txt = "re-auth";     *col = COLOR_ERROR;   break;
    default:                         *txt = "connecting";  *col = COLOR_WARNING; break;
  }
}

// "Updated ..." text, from the millis() stamp of the last successful poll.
static String usg_updated_text() {
  if (g_lastPollMs == 0) return String("updated never");
  uint32_t ago = (millis() - g_lastPollMs) / 1000UL;
  return String("updated ") + format_relative_time(ago).c_str();
}

static void usg_draw_statusbar(const UsageData &d) {
  M5.Lcd.fillRect(0, 0, 320, STATUSBAR_H, COLOR_SURFACE);
  M5.Lcd.setFreeFont(FSS9);
  const int cy = STATUSBAR_H / 2 + 1;

  const char *btxt;
  uint16_t bcol;
  usg_badge(d.status, &btxt, &bcol);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.setTextColor(bcol);
  M5.Lcd.drawString(btxt, PAD_EDGE, cy);

  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.drawString(usg_updated_text(), 195, cy);

  char heap[12];
  snprintf(heap, sizeof(heap), "%uk", (unsigned)(ESP.getFreeHeap() / 1024));
  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.drawString(heap, 320 - PAD_EDGE, cy);
}

static void usg_draw_title(int top, const char *title) {
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString(title, PAD_EDGE, top + USG_TITLE_DY);
}

// True if there are real numbers worth showing: either a poll has succeeded
// this boot (status != UNKNOWN) or last-known-good values were restored from
// NVS (stale). Only a fresh device with an empty NVS shows "--".
static bool usg_has_data(const UsageData &d) {
  return d.status != UsageData::UNKNOWN || d.stale;
}

// The big percentage. With no data at all, "--"; stale data is dimmed.
static void usg_draw_value(int top, const UsageData &d, uint8_t pct) {
  M5.Lcd.fillRect(0, top + USG_VAL_TOP, 320, USG_VAL_H, COLOR_BG);

  if (!usg_has_data(d)) {
    M5.Lcd.setFreeFont(FSSB24);
    M5.Lcd.setTextDatum(ML_DATUM);
    M5.Lcd.setTextColor(COLOR_TEXT_DIM);
    M5.Lcd.drawString("--", PAD_EDGE + 20, top + USG_VAL_TOP + USG_VAL_H / 2);
    return;
  }

  uint16_t col = d.stale ? COLOR_TEXT_DIM : COLOR_PRIMARY;
  char num[6];
  snprintf(num, sizeof(num), "%u", (unsigned)pct);

  int nx = PAD_EDGE + 20;
  int ny = top + USG_VAL_TOP + 4;
  M5.Lcd.setTextColor(col);
  M5.Lcd.setTextFont(7);                       // 7-segment numeric, ~48 px
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.drawString(num, nx, ny);
  int w = M5.Lcd.textWidth(num);

  M5.Lcd.setFreeFont(FSSB18);
  M5.Lcd.setTextDatum(BL_DATUM);
  M5.Lcd.drawString("%", nx + w + 10, ny + 48);
}

static void usg_draw_reset(int top, const UsageData &d, uint32_t reset_s) {
  M5.Lcd.fillRect(0, top + USG_RESET_DY, 320, 22, COLOR_BG);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);

  String t;
  if (!usg_has_data(d))      t = "waiting for first poll";
  else if (reset_s == 0)     t = "resets in --";
  else t = String("resets in ") + format_reset_countdown(reset_s).c_str();
  M5.Lcd.drawString(t, PAD_EDGE + 20, top + USG_RESET_DY);
}

// Full repaint — used on a screen switch into the Usage view.
void ui_show_usage(const UsageData &d) {
  M5.Lcd.fillScreen(COLOR_BG);
  usg_draw_title(USG_CARD1_TOP, "SESSION  (5h)");
  usg_draw_title(USG_CARD2_TOP, "WEEKLY  (7d)");
  M5.Lcd.drawFastHLine(0, USG_CARD2_TOP - 2, 320, COLOR_SURFACE);

  // Invalidate the field cache so ui_update_usage() draws everything.
  s_usg_session = -1;
  s_usg_weekly  = -1;
  s_usg_sreset  = 0xFFFFFFFFUL;
  s_usg_wreset  = 0xFFFFFFFFUL;
  s_usg_status  = -1;
  s_usg_updated = "";
  ui_update_usage(d);
}

// Partial repaint — redraw only the fields whose value changed.
void ui_update_usage(const UsageData &d) {
  String upd = usg_updated_text();
  bool status_changed = ((int)d.status != s_usg_status);

  if (status_changed || upd != s_usg_updated) {
    s_usg_status  = (int)d.status;
    s_usg_updated = upd;
    usg_draw_statusbar(d);
  }

  // A status change can flip stale/UNKNOWN, which affects the value + reset
  // rendering even when the raw numbers are unchanged — so redraw on either.
  int sp = usg_has_data(d) ? (int)d.session_pct : -1;
  if (sp != s_usg_session || status_changed) {
    s_usg_session = sp;
    usg_draw_value(USG_CARD1_TOP, d, d.session_pct);
  }
  if (d.session_reset_s != s_usg_sreset || status_changed) {
    s_usg_sreset = d.session_reset_s;
    usg_draw_reset(USG_CARD1_TOP, d, d.session_reset_s);
  }

  int wp = usg_has_data(d) ? (int)d.weekly_pct : -1;
  if (wp != s_usg_weekly || status_changed) {
    s_usg_weekly = wp;
    usg_draw_value(USG_CARD2_TOP, d, d.weekly_pct);
  }
  if (d.weekly_reset_s != s_usg_wreset || status_changed) {
    s_usg_wreset = d.weekly_reset_s;
    usg_draw_reset(USG_CARD2_TOP, d, d.weekly_reset_s);
  }
}

// --- Onboarding screens ----------------------------------------------------
// The three onboarding steps share one template: a thin header band carrying a
// "STEP n/3" badge and the action verb, then a large centred QR, then two
// detail lines. The QR is sized to dominate the panel and sits in the same
// place on every step, so it stays big and easy to scan throughout.
static void ui_onboard_screen(int step, const char *action, const char *qr,
                              uint8_t qr_version, const String &line1,
                              const String &line2) {
  M5.Lcd.fillScreen(COLOR_BG);

  // Header band — "STEP n/3" (accent) on the left, action verb (dim) right.
  const int HDR = 22;
  M5.Lcd.fillRect(0, 0, 320, HDR, COLOR_SURFACE);
  M5.Lcd.setFreeFont(FSS9);
  char badge[12];
  snprintf(badge, sizeof(badge), "STEP %d/3", step);
  M5.Lcd.setTextColor(COLOR_PRIMARY);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.drawString(badge, PAD_EDGE, HDR / 2);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.drawString(action, 320 - PAD_EDGE, HDR / 2);

  // Large centred QR — fills most of the panel for easy scanning.
  const int qr_w = 168;
  M5.Lcd.qrcode(qr, (320 - qr_w) / 2, HDR + 8, qr_w, qr_version);

  // Two detail lines below the QR.
  int y = HDR + 8 + qr_w + 6;
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString(line1, 160, y);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString(line2, 160, y + 16);
}

// Step 1 — join the device's soft-AP. QR is a WIFI: join string for the open
// AP; the SSID is shown too for manual entry.
void ui_show_provisioning() {
  String ssid = ap_ssid();
  String qr = "WIFI:T:nopass;S:" + ssid + ";;";
  ui_onboard_screen(1, "JOIN WIFI", qr.c_str(), 4,
                    ssid, "or open  192.168.4.1");
}

// Step 2 — a phone has joined the soft-AP. The QR lands straight on the WiFi
// config page (no menu click); the captive portal usually opens it anyway.
void ui_portal_client_connected() {
  ui_onboard_screen(2, "OPEN SETUP", "http://192.168.4.1/wifi", 4,
                    "192.168.4.1", "open if setup did not pop up");
}

// Step 3 — the "Log in with Claude" screen (ADR 007/009). The device is on the
// home network serving the Stage 2 web portal; `portal_url` is the LAN IP plus
// /param, so the QR lands straight on the login + paste page. Re-scanning it
// after the Claude login is the way back to the paste-the-code field.
void ui_show_oauth_login(const String &portal_url) {
  ui_onboard_screen(3, "LOG IN", portal_url.c_str(), 4,
                    portal_url, "log in, then scan again to paste");
}

// Bottom-of-screen error banner — shown when the portal rejects a bad key.
void ui_portal_hint(const char *msg) {
  M5.Lcd.fillRect(0, 240 - 24, 320, 24, COLOR_ERROR);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString(msg, 160, 240 - 12);
}

void ui_show_reset_confirm() {
  M5.Lcd.fillScreen(COLOR_ERROR);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSSB12);
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString("RESET", 160, 100);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.drawString("Keep holding C to wipe config", 160, 138);
}

// Confirm screen for the "change credential" gesture (Task 3.4) — held button
// B. Distinct from the red full-reset screen: this keeps WiFi, re-onboards only
// the Claude login.
void ui_show_reonboard_confirm() {
  M5.Lcd.fillScreen(COLOR_PRIMARY);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSSB12);
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString("RE-ONBOARD", 160, 100);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.drawString("Keep holding B to change login", 160, 138);
}

// Shown when the OAuth credential is dead — the refresh token was revoked or
// rejected, so polling has stopped until the user re-onboards (Task 4.1). The
// fix is the button-B re-onboard gesture (Task 3.4). The revocation line is the
// ADR 008 compensating control — surfaced here and in the README.
void ui_show_reauth_required() {
  M5.Lcd.fillScreen(COLOR_BG);
  ui_header("ACTION NEEDED");
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSS12);
  M5.Lcd.setTextColor(COLOR_ERROR);
  M5.Lcd.drawString("Claude login expired", 160, 60);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString("Usage updates have stopped.", 160, 96);
  M5.Lcd.setTextColor(COLOR_PRIMARY);
  M5.Lcd.drawString("Hold button B 5s to log in again", 160, 124);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString("Lost the device? Revoke its access", 160, 162);
  M5.Lcd.drawString("in your Claude account settings.", 160, 180);
}

// Transient "refreshing" badge — overwrites only the status-bar badge slot
// while a blocking token refresh is in flight. The stale card data underneath
// stays put; the next ui_update_usage() restores the real badge. Meant for the
// Usage screen (the only one with a status bar); the caller guards on that.
void ui_show_refreshing() {
  M5.Lcd.fillRect(0, 0, 130, STATUSBAR_H, COLOR_SURFACE);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.setTextColor(COLOR_WARNING);
  M5.Lcd.drawString("refreshing", PAD_EDGE, STATUSBAR_H / 2 + 1);
}
