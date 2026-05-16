// oauth_pkce.h — PKCE + authorize-URL helpers (pure, host-testable).
//
// The OAuth onboarding flow (ADR 007) is an authorization-code + PKCE exchange.
// This module owns the parts of that with no Arduino / network dependency —
// base64url encoding, percent-encoding, and assembling the authorize URL — so
// the host unit tests (m5clawd/test/run.sh) can exercise them. The pieces that
// need the device (random bytes, the SHA-256 of the PKCE challenge) live in
// oauth.ino. See docs/Phase 3 PHASE_IMP.md Epic 3 and ADR 007.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>

// base64url encode (RFC 4648 section 5), no '=' padding — the encoding PKCE
// and OAuth `state` use. URL-safe alphabet: '+'->'-', '/'->'_'.
std::string pkce_base64url(const uint8_t *data, size_t len);

// Percent-encode per RFC 3986: every byte except the unreserved set
// A-Z a-z 0-9 - . _ ~ becomes %XX (uppercase hex). A space becomes %20, not
// '+'. Used for the authorize URL's redirect_uri and scope query values.
std::string oauth_url_encode(const std::string &s);

// Assemble the Claude Code OAuth authorize URL — an authorization-code request
// with PKCE. redirect_uri and scope are percent-encoded here; client_id,
// code_challenge and state are already URL-safe (base64url / UUID) so they are
// inserted verbatim. `code=true` marks the headless paste-back-code variant of
// the flow (ADR 007) — the success page then displays the code to copy.
std::string oauth_build_authorize_url(const std::string &endpoint,
                                      const std::string &client_id,
                                      const std::string &redirect_uri,
                                      const std::string &scope,
                                      const std::string &code_challenge,
                                      const std::string &state);
