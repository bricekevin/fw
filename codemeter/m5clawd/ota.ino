// ota.ino — OTA via GitHub Releases (see ota.h for the interface, ADR 011
// for the design and trade-offs). Status: TLS cert validation is intentionally
// disabled in v1 (`setInsecure()`); embedding GitHub's root CA chain is a
// follow-up flagged in 4_QUALITY_ASSURANCE.md.
#include "ota.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "config.h"

// Core 1.0.4 notes (informing the workarounds in this file):
//   - WiFiClientSecure has no setInsecure(); it defaults to MBEDTLS_SSL_VERIFY_
//     NONE when no CA cert is supplied, which is what we want for v1.
//   - HTTPClient has no setFollowRedirects(); we manually handle the 302 the
//     GitHub asset URL replies with.
//   - esp_ota_mark_app_valid_cancel_rollback() does not exist (IDF v3.2). A bad
//     image that bricks the boot needs a USB reflash; the ESP32 *boot loader*
//     still falls back to the previous slot if the new image fails to flash
//     correctly, but app-level pending-verify rollback is absent.

#define OTA_GITHUB_API \
    "https://api.github.com/repos/bricekevin/fw/releases/latest"
#define OTA_CHECK_INTERVAL_MS  (6UL * 60UL * 60UL * 1000UL)   // 6 h
#define OTA_USER_AGENT         "codeMeter-OTA/" FW_VERSION

OtaState g_ota;

// State held across tick() invocations while a download is in flight, so the
// loop stays responsive — each tick reads at most one chunk into Update.
struct OtaDownload {
  WiFiClientSecure *client = nullptr;
  HTTPClient       *http   = nullptr;
  size_t            total  = 0;
  size_t            written = 0;
};
static OtaDownload dl;
static char        pendingUrl[256] = "";        // asset browser_download_url
static uint32_t    nextCheckMs     = 0;

// --- internal helpers -------------------------------------------------------

static void ota_dl_cleanup() {
  if (dl.http)   { dl.http->end();  delete dl.http;   dl.http   = nullptr; }
  if (dl.client) {                  delete dl.client; dl.client = nullptr; }
  dl.total = dl.written = 0;
}

static void set_failed(const char *why) {
  g_ota.phase = OtaState::FAILED;
  strncpy(g_ota.lastError, why, sizeof(g_ota.lastError) - 1);
  g_ota.lastError[sizeof(g_ota.lastError) - 1] = 0;
  Serial.printf("[ota] FAILED: %s\n", why);
}

// In a newer ESP-IDF this is where we would call
// esp_ota_mark_app_valid_cancel_rollback() to commit a freshly-installed
// image. Core 1.0.4 has no such API — see the header comment above.
static void ota_confirm_valid_image() { /* no-op on core 1.0.4 */ }

// Lexicographic compare is enough while versions stay vX.Y.Z with one-digit
// fields; promote to component-wise compare when any field can exceed 9.
static const char *strip_v(const char *s) {
  return (s && (s[0] == 'v' || s[0] == 'V')) ? s + 1 : s;
}
static bool ota_is_newer(const char *remote, const char *current) {
  return strcmp(strip_v(remote), strip_v(current)) > 0;
}

// --- public -----------------------------------------------------------------

void ota_begin() {
  ota_confirm_valid_image();
  nextCheckMs = millis() + 8000;           // first check ~8 s after boot
}

const char *ota_phase_str() {
  switch (g_ota.phase) {
    case OtaState::IDLE:        return "idle";
    case OtaState::CHECKING:    return "checking";
    case OtaState::UP_TO_DATE:  return "up to date";
    case OtaState::AVAILABLE:   return "update ready";
    case OtaState::DOWNLOADING: return "downloading";
    case OtaState::INSTALLING:  return "installing";
    case OtaState::FAILED:      return "failed";
  }
  return "?";
}

void ota_check_now() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (g_ota.phase == OtaState::DOWNLOADING ||
      g_ota.phase == OtaState::INSTALLING) return;

  g_ota.phase = OtaState::CHECKING;
  Serial.println("[ota] checking " OTA_GITHUB_API);

  WiFiClientSecure client;                 // no CA cert -> NONE auth (ADR 011)
  HTTPClient http;
  http.setUserAgent(OTA_USER_AGENT);
  http.setTimeout(10000);
  if (!http.begin(client, OTA_GITHUB_API)) {
    set_failed("http begin");
    return;
  }
  int code = http.GET();
  if (code != 200) {
    char err[40];
    snprintf(err, sizeof(err), "HTTP %d on check", code);
    http.end();
    set_failed(err);
    return;
  }

  // The full Release payload is ~5-10 KB but we only need three fields, so
  // filter the parse to keep memory small.
  StaticJsonDocument<256> filter;
  filter["tag_name"] = true;
  JsonObject assetFilter = filter["assets"].createNestedObject();
  assetFilter["name"]                 = true;
  assetFilter["browser_download_url"] = true;

  DynamicJsonDocument doc(2048);
  DeserializationError jerr = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (jerr) { set_failed("json parse"); return; }

  const char *tag = doc["tag_name"] | "";
  const char *url = "";
  for (JsonObject a : doc["assets"].as<JsonArray>()) {
    const char *name = a["name"] | "";
    if (strcmp(name, "firmware.bin") == 0) {
      url = a["browser_download_url"] | "";
      break;
    }
  }
  if (tag[0] == 0) { set_failed("no tag");          return; }
  if (url[0] == 0) { set_failed("no firmware.bin"); return; }

  strncpy(g_ota.latestVersion, tag, sizeof(g_ota.latestVersion) - 1);
  g_ota.latestVersion[sizeof(g_ota.latestVersion) - 1] = 0;
  g_ota.lastCheckMs = millis();
  if (ota_is_newer(tag, FW_VERSION)) {
    strncpy(pendingUrl, url, sizeof(pendingUrl) - 1);
    pendingUrl[sizeof(pendingUrl) - 1] = 0;
    g_ota.phase = OtaState::AVAILABLE;
    Serial.printf("[ota] AVAILABLE: %s (running %s) -> auto-installing\n",
                  tag, FW_VERSION);
    ota_apply_update_now();                  // ADR 011: auto-apply policy
  } else {
    g_ota.phase = OtaState::UP_TO_DATE;
    Serial.printf("[ota] up to date (latest %s, running %s)\n",
                  tag, FW_VERSION);
  }
}

void ota_apply_update_now() {
  if (g_ota.phase != OtaState::AVAILABLE)       return;
  if (pendingUrl[0] == 0)                       { set_failed("no pending url"); return; }
  if (WiFi.status() != WL_CONNECTED)            { set_failed("wifi down");      return; }

  Serial.printf("[ota] downloading %s\n", pendingUrl);
  dl.http = new HTTPClient();
  dl.http->setUserAgent(OTA_USER_AGENT);
  dl.http->setTimeout(15000);

  // GitHub release assets reply 302 to release-assets.githubusercontent.com —
  // core 1.0.4 HTTPClient does not follow redirects, so do it ourselves (up
  // to a small hop count to defeat redirect loops). Allocate a fresh
  // WiFiClientSecure per hop: SNI from a previous handshake persists on the
  // reused client, and the CDN then 301s us back to github.com with the
  // Azure blob path glued to the wrong host -> 404. A fresh client gives
  // each hop a clean TLS handshake with the right SNI.
  const char *headerKeys[] = {"Location"};
  dl.http->collectHeaders(headerKeys, 1);
  String url = pendingUrl;
  int    code = 0;
  for (int hop = 0; hop < 4; ++hop) {       // github -> assets -> blob = 3 hops
    delete dl.client;
    dl.client = new WiFiClientSecure();     // fresh TLS per hop (see comment)
    if (!dl.http->begin(*dl.client, url)) {
      set_failed("http begin (dl)");
      ota_dl_cleanup();
      return;
    }
    code = dl.http->GET();
    if (code == 301 || code == 302 || code == 307 || code == 308) {
      String loc = dl.http->header("Location");
      dl.http->end();
      if (loc.length() == 0) {
        set_failed("redirect, no Location");
        ota_dl_cleanup();
        return;
      }
      Serial.printf("[ota] %d -> %s\n", code, loc.c_str());
      url = loc;
      continue;
    }
    break;                                  // 200 (good) or a real error
  }
  if (code != 200) {
    char err[40];
    snprintf(err, sizeof(err), "HTTP %d on dl", code);
    set_failed(err);
    ota_dl_cleanup();
    return;
  }
  dl.total = (size_t)dl.http->getSize();
  if (dl.total == 0 || dl.total == (size_t)-1) {
    set_failed("no content-length");
    ota_dl_cleanup();
    return;
  }
  if (!Update.begin(dl.total)) {
    set_failed("Update.begin");
    ota_dl_cleanup();
    return;
  }
  dl.written = 0;
  g_ota.downloadPct = 0;
  g_ota.phase = OtaState::DOWNLOADING;
  Serial.printf("[ota] download started, %u bytes\n", (unsigned)dl.total);
}

void ota_tick() {
  uint32_t now = millis();

  // Drive an in-flight download forward in chunks so loop() stays responsive.
  if (g_ota.phase == OtaState::DOWNLOADING) {
    if (!dl.http || !dl.client) { set_failed("dl state lost"); return; }
    WiFiClient *stream = dl.http->getStreamPtr();
    if (!stream) { set_failed("no stream"); ota_dl_cleanup(); return; }
    uint8_t buf[2048];
    int avail = stream->available();
    if (avail > 0) {
      int wanted = avail > (int)sizeof(buf) ? (int)sizeof(buf) : avail;
      int n = stream->readBytes(buf, wanted);
      if (n > 0) {
        size_t w = Update.write(buf, n);
        if (w != (size_t)n) {
          set_failed("Update.write");
          ota_dl_cleanup();
          return;
        }
        dl.written += w;
        g_ota.downloadPct = (uint8_t)((dl.written * 100) / dl.total);
      }
    }
    if (dl.written >= dl.total) {
      g_ota.phase = OtaState::INSTALLING;
      Serial.println("[ota] download complete -> finalising");
      bool ok = Update.end(true);
      ota_dl_cleanup();
      if (!ok) {
        char err[40];
        snprintf(err, sizeof(err), "Update.end err %u", Update.getError());
        set_failed(err);
        return;
      }
      Serial.println("[ota] installed -> reboot");
      delay(500);
      ESP.restart();
    }
    return;
  }

  // Scheduled poll.
  if (now >= nextCheckMs) {
    nextCheckMs = now + OTA_CHECK_INTERVAL_MS;
    ota_check_now();
  }
}
