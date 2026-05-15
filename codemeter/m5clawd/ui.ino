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
  M5.Lcd.drawString("Hold button C 5s to re-onboard", 160, 144);
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

void ui_show_provisioning() {
  M5.Lcd.fillScreen(COLOR_BG);
  ui_header("CONFIGURE WIFI");

  String ssid = ap_ssid();
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setFreeFont(FSS9);

  int x = PAD_EDGE;
  int y = STATUSBAR_H + PAD_SECTION;

  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString("1. Join WiFi network:", x, y);
  y += 22;
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString(ssid, x + 12, y);
  y += 30;
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString("2. Open in a browser:", x, y);
  y += 22;
  M5.Lcd.setTextColor(COLOR_TEXT);
  M5.Lcd.drawString("http://192.168.4.1", x + 12, y);

  // QR code that joins the open soft-AP directly (no password).
  String qr = "WIFI:T:nopass;S:" + ssid + ";;";
  M5.Lcd.qrcode(qr.c_str(), 320 - 100 - PAD_EDGE,
                STATUSBAR_H + PAD_SECTION, 100, 4);
}

void ui_portal_client_connected() {
  M5.Lcd.fillScreen(COLOR_BG);
  ui_header("CONFIGURE WIFI");
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSS12);
  M5.Lcd.setTextColor(COLOR_SUCCESS);
  M5.Lcd.drawString("Phone connected", 160, 104);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(COLOR_TEXT_DIM);
  M5.Lcd.drawString("Open http://192.168.4.1", 160, 138);
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
