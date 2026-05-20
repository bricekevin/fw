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
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <Preferences.h>
#include "config.h"
#include "ota_certs.h"

// Core 1.0.4 notes (informing the workarounds in this file):
//   - WiFiClientSecure has no setInsecure() — we don't need it: TLS is now
//     pinned via setCACert(GITHUB_OTA_ROOTS) (ADR 011, ota_certs.h).
//   - HTTPClient has no setFollowRedirects(); we manually handle the 302 the
//     GitHub asset URL replies with.
//   - esp_ota_mark_app_valid_cancel_rollback() does not exist (IDF v3.2), so
//     we implement app-level rollback ourselves: after an OTA install we set
//     a "pending verify" flag in NVS. On the next boot, if the reset reason
//     looks like a crash (panic / watchdog / brownout) AND the flag is still
//     set, we swap the boot partition back to the previous slot and reboot
//     before any of our crash-prone code runs. The flag is cleared once the
//     first OTA poll on the new image completes (whether the answer is
//     "up to date" or "newer available"), which exercises both WiFi and
//     TLS — a reasonable proxy for "this image works".

#define OTA_NVS_NAMESPACE       "m5clawd_ota"
#define OTA_NVS_PENDING_KEY     "pending"

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

// App-level rollback for core 1.0.4 (which lacks esp_ota_mark_app_valid_
// cancel_rollback). Returns true if a rollback was performed (caller should
// not continue — we reboot).
static bool ota_check_pending_and_rollback() {
  Preferences p;
  if (!p.begin(OTA_NVS_NAMESPACE, /*readonly=*/false)) return false;
  bool pending = p.getBool(OTA_NVS_PENDING_KEY, false);
  if (!pending) { p.end(); return false; }

  esp_reset_reason_t reason = esp_reset_reason();
  const bool crashed = (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
                        reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT ||
                        reason == ESP_RST_BROWNOUT);
  if (!crashed) {
    // Clean boot of the new image — leave the flag set; clear_pending() will
    // remove it once a poll succeeds (the real "this image works" signal).
    p.end();
    return false;
  }

  // The new image crashed before the poll could clear the flag — revert.
  Serial.printf("[ota] crash on pending image (reset reason %d) -> ROLLBACK\n",
                (int)reason);
  p.remove(OTA_NVS_PENDING_KEY);
  p.end();

  const esp_partition_t *running  = esp_ota_get_running_partition();
  const esp_partition_t *previous = esp_ota_get_next_update_partition(running);
  if (!previous || previous == running) {
    Serial.println("[ota] no previous partition to roll back to");
    return false;
  }
  if (esp_ota_set_boot_partition(previous) != ESP_OK) {
    Serial.println("[ota] esp_ota_set_boot_partition failed");
    return false;
  }
  Serial.printf("[ota] rolling back to %s -> reboot\n", previous->label);
  delay(300);
  ESP.restart();
  return true;                               // unreachable
}

// Called by the OTA poller once a poll completes successfully on the new
// image. WiFi + TLS + GitHub all worked, so commit.
static void ota_clear_pending_flag() {
  Preferences p;
  if (!p.begin(OTA_NVS_NAMESPACE, /*readonly=*/false)) return;
  if (p.getBool(OTA_NVS_PENDING_KEY, false)) {
    p.remove(OTA_NVS_PENDING_KEY);
    Serial.println("[ota] new image committed (poll succeeded)");
  }
  p.end();
}

// Set when an OTA install has finished and we're about to reboot into the
// new slot. The next boot will check this + the reset reason.
static void ota_set_pending_flag() {
  Preferences p;
  if (p.begin(OTA_NVS_NAMESPACE, /*readonly=*/false)) {
    p.putBool(OTA_NVS_PENDING_KEY, true);
    p.end();
  }
}

static void ota_confirm_valid_image() { ota_check_pending_and_rollback(); }

// Component-wise compare on major.minor.patch — lexicographic was OK while
// every field stayed single-digit but breaks the moment one hits 10.
static const char *strip_v(const char *s) {
  return (s && (s[0] == 'v' || s[0] == 'V')) ? s + 1 : s;
}
static bool ota_is_newer(const char *remote, const char *current) {
  int rm = 0, rn = 0, rp = 0, cm = 0, cn = 0, cp = 0;
  sscanf(strip_v(remote),  "%d.%d.%d", &rm, &rn, &rp);
  sscanf(strip_v(current), "%d.%d.%d", &cm, &cn, &cp);
  if (rm != cm) return rm > cm;
  if (rn != cn) return rn > cn;
  return rp > cp;
}

// --- public -----------------------------------------------------------------

// Called from the very top of setup() — earlier than ota_begin (which waits
// for WiFi). If the new image is crashing on every boot, the boot-crash loop
// gets broken here BEFORE we touch the radio or anything else that might be
// the actual crash source.
void ota_check_rollback_on_boot() { ota_check_pending_and_rollback(); }

void ota_begin() {
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

  WiFiClientSecure client;
  client.setCACert(GITHUB_OTA_ROOTS);       // pinned trust anchors (ADR 011)
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
  // A poll just succeeded (WiFi + TLS + GitHub all worked), so this image is
  // healthy enough to commit. Clears any pending-verify flag from a prior
  // OTA install so the next crash won't roll us back.
  ota_clear_pending_flag();
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
    dl.client->setCACert(GITHUB_OTA_ROOTS); // pinned trust anchors (ADR 011)
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
      ota_set_pending_flag();              // arms boot-time rollback check
      Serial.println("[ota] installed -> reboot (pending verify)");
      delay(500);
      ESP.restart();
    }
    return;
  }

  // Scheduled poll. Defer until NTP has sync'd — TLS cert validation needs
  // a real wall clock or every notBefore check fails. Retry every ~3 s until
  // the clock is real, then settle into the 6 h cadence.
  if (now >= nextCheckMs) {
    if (poller_time_now() == 0) {
      nextCheckMs = now + 3000;
      return;
    }
    nextCheckMs = now + OTA_CHECK_INTERVAL_MS;
    ota_check_now();
  }
}
