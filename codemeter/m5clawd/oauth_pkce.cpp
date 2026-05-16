// oauth_pkce.cpp — see oauth_pkce.h.

#include "oauth_pkce.h"

std::string pkce_base64url(const uint8_t *data, size_t len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string out;
  out.reserve((len + 2) / 3 * 4);

  size_t i = 0;
  while (i + 3 <= len) {
    uint32_t n = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1] << 8 |
                 (uint32_t)data[i + 2];
    out += alphabet[(n >> 18) & 0x3F];
    out += alphabet[(n >> 12) & 0x3F];
    out += alphabet[(n >> 6) & 0x3F];
    out += alphabet[n & 0x3F];
    i += 3;
  }

  // Tail of 1 or 2 bytes — emit the significant chars only, no '=' padding.
  size_t rem = len - i;
  if (rem == 1) {
    uint32_t n = (uint32_t)data[i] << 16;
    out += alphabet[(n >> 18) & 0x3F];
    out += alphabet[(n >> 12) & 0x3F];
  } else if (rem == 2) {
    uint32_t n = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1] << 8;
    out += alphabet[(n >> 18) & 0x3F];
    out += alphabet[(n >> 12) & 0x3F];
    out += alphabet[(n >> 6) & 0x3F];
  }
  return out;
}

std::string oauth_url_encode(const std::string &s) {
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size() * 3);
  for (unsigned char c : s) {
    bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '-' || c == '.' ||
                      c == '_' || c == '~';
    if (unreserved) {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

std::string oauth_build_authorize_url(const std::string &endpoint,
                                      const std::string &client_id,
                                      const std::string &redirect_uri,
                                      const std::string &scope,
                                      const std::string &code_challenge,
                                      const std::string &state) {
  std::string url = endpoint;
  url += "?code=true";
  url += "&client_id=" + client_id;
  url += "&response_type=code";
  url += "&redirect_uri=" + oauth_url_encode(redirect_uri);
  url += "&scope=" + oauth_url_encode(scope);
  url += "&code_challenge=" + code_challenge;
  url += "&code_challenge_method=S256";
  url += "&state=" + state;
  return url;
}
