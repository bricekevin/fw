// config.h — M5Clawd compile-time configuration.
//
// Shared declarations and constants for the M5Clawd sketch. Included by every
// .ino tab. See docs/2_ARCHITECTURE.md and docs/3_DESIGN_SYSTEM.md.

#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "usage_data.h"      // the shared UsageData struct (pure, host-testable)
#include "state_machine.h"   // PollState / PollOutcome (pure, host-testable)
#include "format_helpers.h"  // countdown / relative-time formatters (pure)
#include "refresh_policy.h"  // OAuth refresh-timing decisions (pure, host-testable)

// ---------------------------------------------------------------------------
// Firmware identity
// ---------------------------------------------------------------------------
#define FW_VERSION "0.0.1"

// ---------------------------------------------------------------------------
// Polling
// ---------------------------------------------------------------------------
// Seconds between Anthropic usage polls. Overridable at runtime via NVS in a
// future phase; this is the default / first-boot value.
#define POLL_INTERVAL_S 60

// Backoff ceiling (seconds) after repeated poll failures.
#define POLL_BACKOFF_MAX_S 300

// HTTP/TLS timeout for a single Anthropic poll (milliseconds).
#define POLL_HTTP_TIMEOUT_MS 15000

// Free-heap warning threshold (bytes). A poll that ends below this logs a
// warning — an early signal of a TLS-buffer leak before the 24 h uptime test.
#define POLL_HEAP_FLOOR 90000

// NTP servers — the device needs wall-clock epoch time because the rate-limit
// reset headers are absolute Unix timestamps (see parse_headers.h).
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"

// ---------------------------------------------------------------------------
// WiFi provisioning (captive portal)
// ---------------------------------------------------------------------------
// Soft-AP SSID base. The last 6 hex digits of the MAC are appended at runtime,
// e.g. "M5Clawd-A1B2C3".
#define WIFI_AP_SSID_PREFIX "M5Clawd"

// ---------------------------------------------------------------------------
// Persistent storage (Preferences / NVS)
// ---------------------------------------------------------------------------
#define NVS_NAMESPACE "m5clawd"
#define NVS_KEY_CONFIGURED "wifi_configured"
#define NVS_KEY_POLL_INTERVAL "poll_interval_s"
// Last-known-good UsageData blob, restored on boot (NVS key <= 15 chars).
#define NVS_KEY_LKG_USAGE "lkg_usage"

// OAuth credential record (ADR 008). The Phase 2 single-string credential is
// superseded by access token + refresh token + access-token expiry. NVS keys
// must stay <= 15 chars.
#define NVS_KEY_OAUTH_AT  "oauth_at"   // access token  (String)
#define NVS_KEY_OAUTH_RT  "oauth_rt"   // refresh token (String)
#define NVS_KEY_OAUTH_EXP "oauth_exp"  // access-token expiry, epoch SECONDS (uint32)
// Legacy pre-Phase-3 key — read only for migration detection. A device holding
// this and no refresh token cannot refresh and is treated as CRED_LEGACY.
#define NVS_KEY_API_KEY "anthropic_key"

// What credential the device currently holds — drives the boot/poll path and
// the "re-onboard required" UI state.
enum CredState {
  CRED_NONE,    // no credential stored at all (fresh device)
  CRED_LEGACY,  // only the pre-Phase-3 single token; no refresh token -> cannot
                // self-refresh -> re-onboard required
  CRED_OAUTH,   // access token + refresh token both present -> can self-refresh
};

// ---------------------------------------------------------------------------
// Anthropic API
// ---------------------------------------------------------------------------
#define ANTHROPIC_HOST "api.anthropic.com"
#define ANTHROPIC_MESSAGES_URL "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_API_VERSION "2023-06-01"
// OAuth beta — the unified 5h/7d rate-limit headers are only attached to
// OAuth (Bearer) requests carrying this beta flag. A plain x-api-key request
// returns 200 but no unified headers (confirmed on hardware, 2026-05-15).
#define ANTHROPIC_OAUTH_BETA "oauth-2025-04-20"
// Identify as a Claude Code client (matches the Clawdmeter reference daemon).
#define ANTHROPIC_USER_AGENT "claude-code/2.1.5"
// Cheapest model — we only want the rate-limit response headers.
#define ANTHROPIC_POLL_MODEL "claude-haiku-4-5-20251001"

// ---------------------------------------------------------------------------
// Claude Code OAuth — token refresh (ADR 007; Phase 3 Epic 1 spike)
// ---------------------------------------------------------------------------
// Production Claude Code OAuth client. Public client — no secret; PKCE is the
// client proof. client_id and endpoint established by the Epic 1 spike; see
// docs/Phase 3/PHASE_IMP.md. These pin third-party OAuth-beta internals — if
// Anthropic changes them, a refresh fails closed and the device asks the user
// to re-onboard. Keeping them here makes a change a one-line edit.
#define OAUTH_CLIENT_ID "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
#define OAUTH_TOKEN_URL "https://platform.claude.com/v1/oauth/token"

// Onboarding endpoints (ADR 007 — authorize-code + PKCE, paste-back code).
// The authorize endpoint is the claude.ai / subscription one: M5Clawd reads
// the Claude Code subscription "unified" rate limits. OAUTH_REDIRECT_URI is the
// manual callback — claude.ai then shows the one-time code for the user to copy
// (the headless flow; the device is not a registrable redirect target).
#define OAUTH_AUTHORIZE_URL "https://claude.com/cai/oauth/authorize"
#define OAUTH_REDIRECT_URI  "https://platform.claude.com/oauth/code/callback"
// OAuth scope. The Claude Code OAuth client accepts exactly two registered
// scope sets — "user:inference" (inference only) or the full
// "user:profile user:inference user:sessions:claude_code user:mcp_servers".
// M5Clawd only polls, so it requests the inference-only set; any other
// combination (e.g. "user:inference user:profile") is rejected by the
// authorize endpoint. Verified against the claude CLI's buildAuthUrl.
#define OAUTH_SCOPE         "user:inference"

// WiFiManagerParameter id for the Epic 3 paste-back one-time code field. The
// "Log in with Claude" block is a custom-HTML parameter and needs no id.
#define PARAM_ID_OAUTH_CODE "oauth_code"

// Refresh the access token this many seconds before it expires. Generous (30
// min) so a flaky network gets many poll-spaced retries before the token
// actually dies. Epic 5.2 may shrink this to force a refresh during testing.
#define REFRESH_MARGIN_S 1800

// HTTP/TLS timeout for a single token-refresh call (milliseconds).
#define REFRESH_HTTP_TIMEOUT_MS 15000

// Backoff after a transient refresh failure: base_s, then exponential
// (base_s << fails), capped at max_s. Consumed by refresh_retry_delay_s().
#define REFRESH_BACKOFF_BASE_S 60
#define REFRESH_BACKOFF_MAX_S 900

// ---------------------------------------------------------------------------
// UI — colors (RGB565, what M5.Lcd draw calls expect)
// Source hex values and rationale: docs/3_DESIGN_SYSTEM.md
// ---------------------------------------------------------------------------
#define COLOR_PRIMARY   0xDBAA  // Claude orange   #DA7756
#define COLOR_BG        0x18E3  // deep charcoal   #1A1815
#define COLOR_SURFACE   0x21A3  // panel surface   #26221E
#define COLOR_TEXT      0xFF9D  // warm white      #F5F0E8
#define COLOR_TEXT_DIM  0x8C50  // warm gray       #8A8780
#define COLOR_SUCCESS   0x6D2F  // calm green      #6EA47A
#define COLOR_WARNING   0xE52A  // amber           #E0A050
#define COLOR_ERROR     0xC2AA  // muted red       #C25450

// ---------------------------------------------------------------------------
// UI — layout (320x240 panel)
// ---------------------------------------------------------------------------
#define PAD_EDGE     8   // outer margin from panel edge
#define PAD_SECTION  16  // between major UI sections
#define PAD_ITEM     4   // between adjacent labels/values
#define STATUSBAR_H  18  // top status bar height

// ---------------------------------------------------------------------------
// Buttons — long-press threshold for the NVS-reset gesture (button C)
// ---------------------------------------------------------------------------
#define RESET_HOLD_MS 5000

// ---------------------------------------------------------------------------
// Shared data model
//
// The UsageData struct moved to usage_data.h (included above) so the pure-logic
// modules and their host unit tests can use it without Arduino headers.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Cross-tab function prototypes
//
// The sketch is split into .ino tabs that Arduino concatenates into one
// translation unit. Declaring the public functions here lets any tab call
// into another regardless of concatenation order.
// ---------------------------------------------------------------------------

// wifi_portal.ino
String ap_ssid();
String getParam(String name);
void   wifi_portal_onboard();       // single soft-AP stage — WiFi creds + token (ADR 010)

// secrets_store.ino
bool        secrets_is_configured();
bool        secrets_has_token();    // a usable Claude token is stored
CredState   secrets_cred_state();
String      secrets_get_access_token();
String      secrets_get_refresh_token();
uint32_t    secrets_get_expires_at();
void        secrets_save_tokens(const String &access, const String &refresh,
                                uint32_t expires_at);
void        secrets_reset();
void        secrets_clear_oauth();
void        onWifiSaved();           // WiFiManager callback — home WiFi creds saved
void        oauthCodeSaveCallback(); // Stage 2 portal — paste-back code submitted
const char *secret_redactor(const String &k);

// oauth.ino
enum RefreshResult {
  REFRESH_OK,            // new access token obtained and persisted
  REFRESH_NO_TOKEN,      // no refresh token stored -> re-onboard required
  REFRESH_INVALID_GRANT, // endpoint rejected the refresh token -> re-onboard
  REFRESH_NET_ERROR,     // transient TLS/HTTP failure -> retry with backoff
};
RefreshResult oauth_refresh();
// Outcome of the onboarding code exchange (Epic 3.2).
enum ExchangeResult {
  EXCHANGE_OK,            // access + refresh token obtained and persisted
  EXCHANGE_BAD_CODE,      // endpoint rejected the code/verifier — user re-pastes
  EXCHANGE_RATE_LIMITED,  // endpoint returned 429 — transient; wait, then retry
  EXCHANGE_NET_ERROR,     // transient TLS/HTTP failure — user retries
};
ExchangeResult oauth_exchange_code(const String &pasted);
// Onboarding (Epic 3). oauth_pkce_begin() mints a fresh code_verifier /
// code_challenge / state for one onboarding session; the others read that
// session state back. The verifier and state are held in RAM only — a reboot
// mid-onboarding discards them and the user restarts the login step (ADR 007).
void   oauth_pkce_begin();
String oauth_authorize_url();
String oauth_pkce_verifier();
String oauth_state_token();

// poller.ino
void        poller_begin();
PollOutcome poller_poll(const String &api_key, UsageData *out);
uint32_t    poller_time_now();

// usage_store.ino
void usage_store_save(const UsageData &d);
bool usage_store_load(UsageData *out);

// ui.ino
void ui_show_splash();
void ui_show_connecting();
void ui_show_status();
void ui_show_usage(const UsageData &d);
void ui_update_usage(const UsageData &d);
void ui_show_provisioning();
void ui_show_oauth_login(const String &portal_url);
void ui_show_wifi_error();
void ui_show_reset_confirm();
void ui_show_reonboard_confirm();
void ui_show_reauth_required();
void ui_show_refreshing();
void ui_portal_hint(const char *msg);
void ui_portal_client_connected();
