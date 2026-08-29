# `skisign.json` is PINNED at 2.1.4 — do not update it

`skisign` was the coaster's old OTA identity. It was renamed to `coaster` on
2026-08-29 (skiLiftSign `docs/decisions/030-coaster-rename-via-midstep.md`).

`skisign.json` exists solely so a coaster that was powered off during the
rename can still cross over whenever it next wakes. It advertises the **midstep
release** 2.1.4, whose firmware carries the new identity and polls
`coaster.json` from its very first boot after installing.

**Leave it exactly as it is.** Pointing it at a later coaster release would
work, but it would also mean maintaining two manifests for one product forever,
and the whole purpose of the midstep is that this file never needs touching
again. Current coaster releases go to `coaster.json` only, tagged `coaster-v*`.

`docs/skisign/` (the old web-flasher page) is frozen for the same reason:
existing links keep working, and anything flashed from it OTAs across via this
file.
