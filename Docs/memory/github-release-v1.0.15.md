# GitHub v1.0.15 publication and cleanup — October 14, 2026

Published [v1.0.15](https://github.com/timmyty/sim-copter-remake/releases/tag/v1.0.15) from source commit `57ae3d5239ddf080b8433ed3d60980c3207c33ec`.
Applied the scoped live-lighting and tour-overlay changes to the publication
checkout's `daf151d` baseline. The main development tree contains other local
changes that are not part of this release. Existing publication Content asset
modifications were preserved and left unstaged.

Editor, Shipping and Development builds passed. The full suite passed 523/524;
the one settings persistence failure was caused by the multiprocess runner
disabling config writes. It passed unchanged with `-MultiprocessSaveConfig` and
an isolated profile. All 524 distinct cases have passing final results. The
offscreen D3D12 lighting fixture passed and its image was inspected; both staged
executables passed separate hidden 30-second startup checks. No interactive
playthrough was performed.

The Shipping and matching optional Development archives were extracted and every
file hash compared with staging. Cooked runtime content is byte-identical to
v1.0.14 (900 unchanged runtime files). Public release, tag, downloads and server
SHA-256 digests were verified. Full evidence: `Docs/scratchpad/release-v1.0.15/`.

At the user's request, removed 12 old releases (v1.0.2–v1.0.13)
and the two v1.0.14 game/diagnostic archives, reclaiming
21,807,368,848 bytes (21.81 GB).
The v1.0.14 release remains the optional music library, with its 26
assets and existing music URLs. Updated its title, body and checksums; music pack
IDs/hashes and all Git tags were verified unchanged. The latest release is v1.0.15.

The installed playable copy remains the main development build installed earlier
for the lighting task; publication did not replace the user's installation.
