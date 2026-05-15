// usage_store.ino — last-known-good UsageData persistence (NVS).
//
// After each successful poll the firmware stores the UsageData blob so a cold
// boot (even with WiFi down) can show the last numbers, flagged stale, instead
// of "--". This is separate from secrets_store.ino — the usage snapshot is not
// a secret. Both share the `m5clawd` Preferences namespace.
//
// The whole struct is written as one putBytes() blob keyed by NVS_KEY_LKG_USAGE.
// usage_store_load() rejects a blob whose size does not match the current
// struct, so a future change to UsageData simply discards the stale snapshot
// rather than reading garbage.

void usage_store_save(const UsageData &d) {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putBytes(NVS_KEY_LKG_USAGE, &d, sizeof(UsageData));
  preferences.end();
}

bool usage_store_load(UsageData *out) {
  preferences.begin(NVS_NAMESPACE, true);              // read-only
  size_t stored = preferences.getBytesLength(NVS_KEY_LKG_USAGE);
  bool ok = false;
  if (stored == sizeof(UsageData)) {
    preferences.getBytes(NVS_KEY_LKG_USAGE, out, sizeof(UsageData));
    ok = true;
  }
  preferences.end();
  return ok;
}
