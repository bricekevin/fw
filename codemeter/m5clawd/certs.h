// certs.h — TLS trust anchor for api.anthropic.com.
//
// Probed 2026-05-15 (openssl s_client). api.anthropic.com presents:
//
//   leaf  CN=api.anthropic.com
//     <- WE1            (Google Trust Services intermediate, ECDSA)
//       <- GTS Root R4  (Google Trust Services LLC)
//
// We pin the SELF-SIGNED GTS Root R4 (subject == issuer == "GTS Root R4",
// valid 2016-06-22 .. 2036-06-22, SHA-256
// 34:9D:FA:40:58:C5:E2:63:12:3B:39:8A:E7:95:57:3C:4E:13:13:C8:3F:E6:8F:93:
// 55:6C:D5:E8:03:1B:3C:7D), fetched from https://pki.goog/repo/certs/gtsr4.pem.
//
// The server actually sends a CROSS-SIGNED GTS Root R4 (issued by GlobalSign
// Root CA, expiring 2028-01-28) in its chain. Pinning the self-signed root —
// same subject and public key — validates the chain without depending on that
// shorter-lived cross-sign. Verified end-to-end: openssl s_client against the
// live host with this cert as -CAfile returns "Verify return code: 0 (ok)".
//
// PHASE_PRD note: the PRD assumed Amazon Root CA 1 / ISRG Root X1 and planned
// to bundle two roots as insurance against guessing. The live probe (Task 2.1)
// removed the guesswork — Anthropic uses Google Trust Services — so one
// correctly identified root replaces two guessed ones. If Anthropic ever
// rotates to a different root the handshake fails closed (the poller surfaces
// API_UNREACHABLE); see QA KI-03. The planned OTA path is the recovery route.
// Never substitute WiFiClientSecure::setInsecure() for a stale root.

#pragma once

// Self-signed GTS Root R4 (Google Trust Services LLC).
static const char ANTHROPIC_ROOT_CA[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
    "VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
    "A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
    "WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
    "IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
    "AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
    "QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
    "HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
    "BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
    "9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
    "p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
    "-----END CERTIFICATE-----\n";
