// oauth_pkce_test.cpp — host unit tests for the PKCE / authorize-URL helpers.

#include "test_util.h"
#include "oauth_pkce.h"

static std::string b64url(const std::string &s) {
  return pkce_base64url(reinterpret_cast<const uint8_t *>(s.data()), s.size());
}

static void test_base64url_rfc4648_vectors() {
  // The RFC 4648 section 10 test vectors, padding stripped.
  CHECK_EQ_STR(b64url(""), "");
  CHECK_EQ_STR(b64url("f"), "Zg");
  CHECK_EQ_STR(b64url("fo"), "Zm8");
  CHECK_EQ_STR(b64url("foo"), "Zm9v");
  CHECK_EQ_STR(b64url("foob"), "Zm9vYg");
  CHECK_EQ_STR(b64url("fooba"), "Zm9vYmE");
  CHECK_EQ_STR(b64url("foobar"), "Zm9vYmFy");
}

static void test_base64url_is_url_safe() {
  // Bytes chosen so standard base64 would emit '+' and '/': must come back
  // as '-' and '_' instead, and never carry '=' padding.
  const uint8_t plus_slash[] = {0xFB, 0xFF};
  CHECK_EQ_STR(pkce_base64url(plus_slash, 2), "-_8");

  const uint8_t all_ones[] = {0xFF, 0xFF, 0xFF};
  CHECK_EQ_STR(pkce_base64url(all_ones, 3), "____");

  // A 32-byte verifier (the device's case) base64url-encodes to 43 chars.
  uint8_t verifier[32];
  for (int i = 0; i < 32; ++i) verifier[i] = (uint8_t)(i * 7 + 1);
  std::string encoded = pkce_base64url(verifier, 32);
  CHECK_EQ_INT((int)encoded.size(), 43);
  for (char c : encoded) {
    bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_';
    CHECK(safe);
  }
}

static void test_url_encode() {
  // Unreserved set passes through untouched.
  CHECK_EQ_STR(oauth_url_encode("Az09-._~"), "Az09-._~");
  // Reserved chars are percent-encoded with uppercase hex.
  CHECK_EQ_STR(oauth_url_encode("a b"), "a%20b");
  CHECK_EQ_STR(oauth_url_encode("user:inference user:profile"),
               "user%3Ainference%20user%3Aprofile");
  CHECK_EQ_STR(oauth_url_encode("https://platform.claude.com/oauth/code/callback"),
               "https%3A%2F%2Fplatform.claude.com%2Foauth%2Fcode%2Fcallback");
}

static void test_build_authorize_url() {
  std::string url = oauth_build_authorize_url(
      "https://claude.com/cai/oauth/authorize", "CID",
      "https://x.test/cb", "a b", "CHALLENGE", "STATE");
  CHECK_EQ_STR(
      url,
      "https://claude.com/cai/oauth/authorize?code=true&client_id=CID"
      "&response_type=code&redirect_uri=https%3A%2F%2Fx.test%2Fcb"
      "&scope=a%20b&code_challenge=CHALLENGE&code_challenge_method=S256"
      "&state=STATE");
}

int main() {
  test_base64url_rfc4648_vectors();
  test_base64url_is_url_safe();
  test_url_encode();
  test_build_authorize_url();
  TEST_SUMMARY("oauth_pkce");
}
