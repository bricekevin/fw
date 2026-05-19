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
  M5.Lcd.setFreeFont(FSSB18);
  M5.Lcd.setTextColor(COLOR_PRIMARY);
  M5.Lcd.drawString("codeMeter", 160, 110);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString("v" FW_VERSION, 160, 144);
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
// Card-based layout (320x240): a 32px header ("codeMeter" + a WiFi-signal icon
// + a poll-status dot), then two rounded surface cards (SESSION / WEEKLY).
// Each card carries a title, a large percentage, a pill progress bar, a thin
// elapsed-time marker beneath the bar, and a reset line (absolute time +
// countdown). As a window's spending runs ahead of the clock, the number, the
// elapsed marker, and the card outline shift through three tiers — normal,
// amber (ahead of pace), red (clearly overspending). See docs/3_DESIGN_SYSTEM.md.
//
// ui_show_usage() paints the whole screen (header + card chrome); the
// per-field cache lets ui_update_usage() repaint only the regions that
// changed, so a per-poll refresh never flickers the cards.
//
// The offsets below are hand-tuned for the panel; adjust against a photo
// (the embedded /5_visual step) if a card looks cramped.

#include <time.h>

#define USG_HDR_H      32                    // header band height
#define USG_CARD_X     PAD_EDGE              // 8
#define USG_CARD_W     (320 - 2 * PAD_EDGE)  // 304
#define USG_CARD_H     96
#define USG_CARD1_Y    36
#define USG_CARD2_Y    138
#define USG_CARD_R     10                    // card corner radius
#define USG_PAD        16                    // inner card padding
#define USG_TITLE_DY   9                     // card-title y within a card
#define USG_ROW_DY     34                    // value/bar row top within a card
#define USG_ROW_H      26                    // value/bar row height (clear span)
#define USG_NUM_W      84                    // reserved width for the percentage
#define USG_BAR_H      14                    // pill progress-bar height
#define USG_RESET_DY   68                    // reset line y within a card

// Pace-check (danger highlight) parameters. The window lengths are the fixed
// Anthropic rate-limit periods; the margin keeps a freshly reset window from
// flashing red the moment it is first touched.
#define USG_SESSION_WINDOW_S  (5UL * 3600UL)         // 5-hour window
#define USG_WEEKLY_WINDOW_S   (7UL * 24UL * 3600UL)  // 7-day window
#define USG_DANGER_MARGIN     10                     // pace buffer, percent

// Per-field cache so ui_update_usage() can skip unchanged fields.
static int      s_usg_session  = -1;
static int      s_usg_weekly   = -1;
static uint32_t s_usg_sreset   = 0xFFFFFFFFUL;
static uint32_t s_usg_wreset   = 0xFFFFFFFFUL;
static int      s_usg_status   = -1;
static int      s_usg_rssi     = -999;          // last-drawn WiFi-signal bucket
static int      s_usg_space    = -1;            // last-drawn session pace level
static int      s_usg_wpace    = -1;            // last-drawn weekly pace level
static String   s_usg_clock    = "";            // last-drawn header clock text

// Pace level — how a window's spending compares with the clock. Budget-left%
// and time-left% are both fractions of the whole window:
//   0  on or behind pace          (budget-left >= time-left)
//   1  ahead of pace, in buffer    (within USG_DANGER_MARGIN points)
//   2  clearly overspending        (more than USG_DANGER_MARGIN points ahead)
// The buffer keeps a freshly reset window from jumping straight to red.
static int usg_pace_level(uint8_t cur_pct, uint32_t reset_s,
                          uint32_t window_s) {
  if (reset_s == 0 || window_s == 0) return 0;         // no clock / no window
  int budget_left = 100 - (int)cur_pct;
  int time_left = (int)((uint64_t)reset_s * 100UL / window_s);
  if (time_left > 100) time_left = 100;                // reset_s can exceed it
  if (budget_left < time_left - USG_DANGER_MARGIN) return 2;
  if (budget_left < time_left)                     return 1;
  return 0;
}

// The poll-status dot color: green ok, amber connecting / rate-limited, red
// for any hard error.
static uint16_t usg_status_color(UsageData::Status st) {
  switch (st) {
    case UsageData::OK:           return COLOR_SUCCESS;
    case UsageData::RATE_LIMITED: return COLOR_WARNING;
    case UsageData::UNKNOWN:      return COLOR_WARNING;   // still connecting
    default:                      return COLOR_ERROR;
  }
}

// WiFi signal strength as a 0..4 bucket from the current RSSI.
static int usg_wifi_bars() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

// A pill-shaped progress bar: a recessed dark track with a rounded fill. The
// fill is clamped to at least one bar-height so a low percentage still reads
// as a visible pill rather than a dot.
static void usg_draw_bar(int x, int y, int w, int h, uint8_t pct,
                         uint16_t fill) {
  int r = h / 2;
  M5.Lcd.fillRoundRect(x, y, w, h, r, COLOR_BG);
  if (pct > 100) pct = 100;
  if (pct > 0) {
    int fw = (int)((long)w * pct / 100);
    if (fw < h) fw = h;
    M5.Lcd.fillRoundRect(x, y, fw, h, r, fill);
  }
}

// The "codeMeter" wordmark — static, drawn once on a full repaint.
static void usg_draw_header_title() {
  M5.Lcd.setFreeFont(FSSB12);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.setTextColor(COLOR_PRIMARY, COLOR_BG);
  M5.Lcd.drawString("codeMeter", PAD_EDGE, USG_HDR_H / 2);
}

// The current local time, "3:47pm" style. Empty until NTP has synced.
static String usg_clock_text() {
  time_t now = time(nullptr);
  if (now < 1600000000) return String("");
  struct tm tmv;
  localtime_r(&now, &tmv);
  int h12 = tmv.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  const char *ap = tmv.tm_hour < 12 ? "am" : "pm";
  char buf[12];
  snprintf(buf, sizeof(buf), "%d:%02d%s", h12, tmv.tm_min, ap);
  return String(buf);
}

// The dynamic right side of the header — the local clock, a 4-bar WiFi-signal
// icon, and a small poll-status dot hard in the top-right corner.
static void usg_draw_header_status(const UsageData &d) {
  M5.Lcd.fillRect(160, 0, 320 - 160, USG_HDR_H, COLOR_BG);

  // Local clock, just left of the WiFi icon.
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
  M5.Lcd.drawString(usg_clock_text(), 262, 15);

  // WiFi-signal icon: four rising bars, lit according to signal strength.
  int bars = usg_wifi_bars();
  const int baseY = 23;
  for (int i = 0; i < 4; i++) {
    int bh = 4 + i * 3;
    int bx = 270 + i * 5;
    M5.Lcd.fillRect(bx, baseY - bh, 3, bh,
                    i < bars ? COLOR_TEXT : COLOR_TEXT_DIM);
  }

  // Poll-status dot.
  M5.Lcd.fillCircle(320 - PAD_EDGE - 4, 15, 5, usg_status_color(d.status));
}

static void usg_draw_card_title(int cardY, const char *title) {
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
  M5.Lcd.drawString(title, USG_CARD_X + USG_PAD, cardY + USG_TITLE_DY);
}

// The card alert state — the outline plus a top-right corner dot. Amber when
// the window is ahead of pace, red when clearly overspending; at normal pace
// both are the surface color (invisible, flush with the card).
static void usg_draw_card_frame(int cardY, int pace) {
  uint16_t c = pace >= 2 ? COLOR_ERROR
             : pace == 1 ? COLOR_WARNING
                         : COLOR_SURFACE;
  M5.Lcd.drawRoundRect(USG_CARD_X, cardY, USG_CARD_W, USG_CARD_H,
                       USG_CARD_R, c);
  M5.Lcd.drawRoundRect(USG_CARD_X + 1, cardY + 1, USG_CARD_W - 2,
                       USG_CARD_H - 2, USG_CARD_R - 1, c);
  M5.Lcd.fillCircle(USG_CARD_X + USG_CARD_W - 18, cardY + 15, 6, c);
}

// True if there are real numbers worth showing: either a poll has succeeded
// this boot (status != UNKNOWN) or last-known-good values were restored from
// NVS (stale). Only a fresh device with an empty NVS shows "--".
static bool usg_has_data(const UsageData &d) {
  return d.status != UsageData::UNKNOWN || d.stale;
}

// The percentage + pill bar row, with a thin elapsed-time marker beneath the
// bar (drawn to the same scale): when the orange fill reaches past the marker,
// usage has outrun the clock. `pace` (0/1/2) tints the number and the marker
// amber then red. Cleared and repainted as a unit so the parts never tear
// against each other on a partial update.
static void usg_draw_value(int cardY, const UsageData &d, uint8_t pct,
                           int pace, uint32_t reset_s, uint32_t window_s) {
  int rowY = cardY + USG_ROW_DY;
  M5.Lcd.fillRect(USG_CARD_X + 4, rowY, USG_CARD_W - 8, USG_ROW_H,
                  COLOR_SURFACE);

  int numX = USG_CARD_X + USG_PAD;
  int barX = numX + USG_NUM_W + 8;
  int barW = USG_CARD_X + USG_CARD_W - USG_PAD - barX;
  // The bar + 2px gap + 3px marker form one group, vertically centred.
  int barY = rowY + (USG_ROW_H - USG_BAR_H - 5) / 2;

  M5.Lcd.setFreeFont(FSSB18);
  M5.Lcd.setTextDatum(TL_DATUM);

  if (!usg_has_data(d)) {
    M5.Lcd.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    M5.Lcd.drawString("--", numX, rowY);
    usg_draw_bar(barX, barY, barW, USG_BAR_H, 0, COLOR_PRIMARY);
    return;
  }

  uint16_t numCol = pace >= 2 ? COLOR_ERROR
                  : pace == 1 ? COLOR_WARNING
                  : d.stale   ? COLOR_TEXT_DIM
                              : COLOR_TEXT;
  char num[8];
  snprintf(num, sizeof(num), "%u%%", (unsigned)pct);
  M5.Lcd.setTextColor(numCol, COLOR_SURFACE);
  M5.Lcd.drawString(num, numX, rowY);
  // The bar fill is always Claude orange (dimmed when stale); warning colour
  // lives in the number, marker, and outline via the pace tiers.
  usg_draw_bar(barX, barY, barW, USG_BAR_H, pct,
               d.stale ? COLOR_TEXT_DIM : COLOR_PRIMARY);

  // Elapsed-time marker — fraction of the window already gone, drawn to the
  // same scale as the bar: a dark track (matching the bar's track) with a grey
  // elapsed fill that goes amber then red as the window outpaces the clock.
  // Skipped until NTP has resolved a reset time.
  if (reset_s > 0 && window_s > 0) {
    int markY = barY + USG_BAR_H + 2;
    M5.Lcd.fillRect(barX, markY, barW, 3, COLOR_BG);
    uint32_t elapsed = reset_s >= window_s ? 0 : (window_s - reset_s);
    int ew = (int)((uint64_t)barW * elapsed / window_s);
    uint16_t mc = pace >= 2 ? COLOR_ERROR
                : pace == 1 ? COLOR_WARNING
                            : COLOR_TIME;
    if (ew > 0) M5.Lcd.fillRect(barX, markY, ew, 3, mc);
  }
}

static const char *const USG_WDAY[7] =
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// The absolute wall-clock moment a window resets, in the device's local
// timezone (config.h TZ_POSIX). Same-day resets read "4:35pm"; resets a day or
// more out read "Thu 9am". "--" until NTP has synced the clock.
static String usg_abs_time(uint32_t reset_s) {
  time_t now = time(nullptr);
  if (now < 1600000000) return String("--");
  time_t rt = now + (time_t)reset_s;
  struct tm tmv;
  localtime_r(&rt, &tmv);
  int h12 = tmv.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  const char *ap = tmv.tm_hour < 12 ? "am" : "pm";
  char buf[24];
  if (reset_s < 86400UL)
    snprintf(buf, sizeof(buf), "%d:%02d%s", h12, tmv.tm_min, ap);
  else
    snprintf(buf, sizeof(buf), "%s %d%s", USG_WDAY[tmv.tm_wday % 7], h12, ap);
  return String(buf);
}

// The reset line — the absolute reset time and the countdown, condensed onto
// one line, e.g. "resets 4:35pm   in 22m".
static void usg_draw_reset(int cardY, const UsageData &d, uint32_t reset_s) {
  int y = cardY + USG_RESET_DY;
  M5.Lcd.fillRect(USG_CARD_X + 4, y, USG_CARD_W - 8, 18, COLOR_SURFACE);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);

  String t;
  if (!usg_has_data(d))
    t = "waiting for first poll";
  else if (reset_s == 0)
    t = "resets in --";
  else
    t = String("resets ") + usg_abs_time(reset_s) + "   in " +
        format_reset_countdown(reset_s).c_str();
  M5.Lcd.drawString(t, USG_CARD_X + USG_PAD, y);
}

// Full repaint — used on a screen switch into the Usage view.
void ui_show_usage(const UsageData &d) {
  M5.Lcd.fillScreen(COLOR_BG);
  usg_draw_header_title();

  // Card chrome — rounded surface panels, drawn once.
  M5.Lcd.fillRoundRect(USG_CARD_X, USG_CARD1_Y, USG_CARD_W, USG_CARD_H,
                       USG_CARD_R, COLOR_SURFACE);
  M5.Lcd.fillRoundRect(USG_CARD_X, USG_CARD2_Y, USG_CARD_W, USG_CARD_H,
                       USG_CARD_R, COLOR_SURFACE);
  usg_draw_card_title(USG_CARD1_Y, "SESSION  (5h)");
  usg_draw_card_title(USG_CARD2_Y, "WEEKLY  (7d)");

  // Invalidate every cache so ui_update_usage() repaints the whole screen.
  s_usg_session  = -1;
  s_usg_weekly   = -1;
  s_usg_sreset   = 0xFFFFFFFFUL;
  s_usg_wreset   = 0xFFFFFFFFUL;
  s_usg_status   = -1;
  s_usg_rssi     = -999;
  s_usg_space    = -1;
  s_usg_wpace    = -1;
  s_usg_clock    = "";
  ui_update_usage(d);
}

// Partial repaint — redraw only the regions whose value changed.
void ui_update_usage(const UsageData &d) {
  // Header — redraw on a status change, a WiFi-signal bucket change, or when
  // the clock ticks to a new minute.
  int bars = usg_wifi_bars();
  String clk = usg_clock_text();
  bool status_changed = ((int)d.status != s_usg_status);
  if (status_changed || bars != s_usg_rssi || clk != s_usg_clock) {
    s_usg_status = (int)d.status;
    s_usg_rssi   = bars;
    s_usg_clock  = clk;
    usg_draw_header_status(d);
  }

  // Pace level (0 normal / 1 amber / 2 red), per window.
  int space = usg_pace_level(d.session_pct, d.session_reset_s,
                             USG_SESSION_WINDOW_S);
  int wpace = usg_pace_level(d.weekly_pct, d.weekly_reset_s,
                             USG_WEEKLY_WINDOW_S);
  bool space_changed = (space != s_usg_space);
  bool wpace_changed = (wpace != s_usg_wpace);

  // reset_s shrinks every poll, so it also drives the bar's elapsed-time
  // marker — the value row is redrawn whenever it changes. A status change can
  // flip stale/UNKNOWN, which affects rendering even with unchanged numbers.

  // --- Session card ---
  bool sreset_changed = (d.session_reset_s != s_usg_sreset);
  if (space_changed) {
    s_usg_space = space;
    usg_draw_card_frame(USG_CARD1_Y, space);
  }
  int sp = usg_has_data(d) ? (int)d.session_pct : -1;
  if (sp != s_usg_session || status_changed || space_changed ||
      sreset_changed) {
    s_usg_session = sp;
    usg_draw_value(USG_CARD1_Y, d, d.session_pct, space,
                   d.session_reset_s, USG_SESSION_WINDOW_S);
  }
  if (sreset_changed || status_changed) {
    s_usg_sreset = d.session_reset_s;
    usg_draw_reset(USG_CARD1_Y, d, d.session_reset_s);
  }

  // --- Weekly card ---
  bool wreset_changed = (d.weekly_reset_s != s_usg_wreset);
  if (wpace_changed) {
    s_usg_wpace = wpace;
    usg_draw_card_frame(USG_CARD2_Y, wpace);
  }
  int wp = usg_has_data(d) ? (int)d.weekly_pct : -1;
  if (wp != s_usg_weekly || status_changed || wpace_changed ||
      wreset_changed) {
    s_usg_weekly = wp;
    usg_draw_value(USG_CARD2_Y, d, d.weekly_pct, wpace,
                   d.weekly_reset_s, USG_WEEKLY_WINDOW_S);
  }
  if (wreset_changed || status_changed) {
    s_usg_wreset = d.weekly_reset_s;
    usg_draw_reset(USG_CARD2_Y, d, d.weekly_reset_s);
  }
}

// Brightness overlay — a brief centred readout shown when button B cycles the
// screen brightness: a sun icon above a segmented level bar. It is wiped by
// the next full screen repaint.
void ui_show_brightness(int level, int count) {
  const int bw = 170, bh = 104;
  const int bx = (320 - bw) / 2, by = (240 - bh) / 2;
  M5.Lcd.fillRoundRect(bx, by, bw, bh, 12, COLOR_SURFACE);
  M5.Lcd.drawRoundRect(bx, by, bw, bh, 12, COLOR_PRIMARY);

  // Sun icon — a disc with eight rays.
  const int cx = 160, cy = by + 34;
  M5.Lcd.fillCircle(cx, cy, 9, COLOR_PRIMARY);
  for (int i = 0; i < 8; i++) {
    float a = i * (PI / 4.0f);
    M5.Lcd.drawLine(cx + (int)(13 * cosf(a)), cy + (int)(13 * sinf(a)),
                    cx + (int)(19 * cosf(a)), cy + (int)(19 * sinf(a)),
                    COLOR_PRIMARY);
  }

  // Segmented level bar — (level + 1) of `count` segments lit.
  const int segW = 26, gap = 8, segH = 16;
  int totalW = count * segW + (count - 1) * gap;
  int sx = (320 - totalW) / 2, sy = by + bh - 34;
  for (int i = 0; i < count; i++)
    M5.Lcd.fillRoundRect(sx + i * (segW + gap), sy, segW, segH, 3,
                         i <= level ? COLOR_PRIMARY : COLOR_BG);
}

// 13x13 pixel-art Claude "spark" — an eight-ray burst. One bit per pixel,
// MSB-first: bit (12 - x) is column x.
static const uint16_t CLAUDE_SPRITE[13] = {
  0x0040, 0x0842, 0x0444, 0x0248, 0x0150, 0x00E0, 0x1FFF,
  0x00E0, 0x0150, 0x0248, 0x0444, 0x0842, 0x0040,
};

// Draw the snake head — a dark cell carrying the orange Claude spark — with
// its top-left corner at pixel (cx, cy) inside a cell of size g.
static void snake_draw_head(int cx, int cy, int g) {
  M5.Lcd.fillRect(cx, cy, g, g, COLOR_BG);
  int ox = cx + (g - 13) / 2, oy = cy + (g - 13) / 2;
  for (int r = 0; r < 13; r++)
    for (int c = 0; c < 13; c++)
      if ((CLAUDE_SPRITE[r] >> (12 - c)) & 1)
        M5.Lcd.drawPixel(ox + c, oy + r, COLOR_PRIMARY);
}

// One pass of the easter-egg snake — a randomised depth-first wander that
// lays a `trail`-coloured trail until every cell of the panel is covered.
static void snake_walk(uint16_t trail) {
  const int G = 16, COLS = 20, ROWS = 15, N = COLS * ROWS;   // 300 cells
  static bool     visited[300];
  static uint16_t stack[300];
  for (int i = 0; i < N; i++) visited[i] = false;

  int sp = 0, start = random(N);
  visited[start] = true;
  stack[sp++] = start;
  snake_draw_head((start % COLS) * G, (start / COLS) * G, G);
  int prev = start;

  while (sp > 0) {
    int cur = stack[sp - 1];
    int cr = cur / COLS, cc = cur % COLS;

    // Unvisited orthogonal neighbours of the current cell.
    int cand[4], nc = 0;
    if (cr > 0        && !visited[cur - COLS]) cand[nc++] = cur - COLS;
    if (cr < ROWS - 1 && !visited[cur + COLS]) cand[nc++] = cur + COLS;
    if (cc > 0        && !visited[cur - 1])    cand[nc++] = cur - 1;
    if (cc < COLS - 1 && !visited[cur + 1])    cand[nc++] = cur + 1;

    int next;
    if (nc > 0) {                              // wander into a random neighbour
      next = cand[random(nc)];
      visited[next] = true;
      stack[sp++] = next;
    } else {                                   // dead end -> backtrack
      sp--;
      next = (sp > 0) ? stack[sp - 1] : -1;
    }

    // The cell the head leaves becomes trail; the head moves on.
    M5.Lcd.fillRect((prev % COLS) * G, (prev / COLS) * G, G, G, trail);
    if (next >= 0) snake_draw_head((next % COLS) * G, (next / COLS) * G, G);
    prev = next;
    delay(4);
  }
}

// Easter egg — a thin snake led by the pixel-art Claude spark wanders the
// panel at random, eating it. Two passes: first it fills the screen orange,
// then a second wander turns it all to black, then the caller repaints.
// Triggered by holding all three buttons at once. Blocking, ~8 s.
void ui_easter_snake() {
  randomSeed(millis());
  snake_walk(COLOR_PRIMARY);                   // eat the screen orange
  delay(350);
  snake_walk(COLOR_BG);                        // eat the orange to black
  delay(350);
}

// --- Onboarding screens ----------------------------------------------------
// The two onboarding screens share one template: a header band with the
// "codeMeter" name, a large centred QR, and two detail lines.
// Identical placement keeps the QR big and stationary. The captive-portal
// page (not the LCD) carries the full step-by-step guide.
static void ui_onboard_screen(const char *qr, uint8_t qr_version,
                              const String &line1, const String &line2) {
  M5.Lcd.fillScreen(COLOR_BG);

  // Header band — the product name, centred.
  const int HDR = 22;
  M5.Lcd.fillRect(0, 0, 320, HDR, COLOR_SURFACE);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(COLOR_PRIMARY);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.drawString("codeMeter", 160, HDR / 2);

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

// First screen — join the device's setup WiFi. QR is a WIFI: join string for
// the open soft-AP; the SSID is shown too for joining by hand.
void ui_show_provisioning() {
  String ssid = ap_ssid();
  String qr = "WIFI:T:nopass;S:" + ssid + ";;";
  ui_onboard_screen(qr.c_str(), 4,
                    ssid, "Join this Wi-Fi, then open the setup page");
}

// Second screen — a phone has joined the soft-AP. The QR opens the setup page,
// which carries the full step-by-step instructions.
void ui_portal_client_connected() {
  ui_onboard_screen("http://192.168.4.1/wifi", 4,
                    "192.168.4.1", "Open this address to set up the device");
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
