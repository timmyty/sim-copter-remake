# Turbo HUD — October 14, 2026

Turbo Mode replaces the Sprint Mode name in game code and documentation.

- The minimap's existing left-side lamp/switch location displays TURBO and ON/OFF.
  ON lights the red lamp and flips the switch up; OFF dims it and flips it down.
  Keyboard/mouse and controller input share exactly the same status display.
- A larger silver header directly above the map displays yellow stencil lettering:
  F1: Controls for keyboard/mouse, L3 + R3: Show help for controller. Expanded help
  changes the wording to Hide controls / Hide help. Either stick press order works.
- The old collapsed help text and Ctrl+P Photo Mode help overlay are removed.
  Photo Mode itself and its Ctrl+P, Esc-menu and F12 controls are unchanged.
- On foot, no Turbo status or Turbo help rows are shown. The parachute status
  readout is removed. Pilot health, running and parachute controls are unchanged.
- Map content, buttons and mission label retain their existing positions within
  the map body; hit testing accounts for the added header. The plates never take
  keyboard focus or capture input.

Turbo still toggles with Shift or L3 on release. Recall, map and help chords do not
change it; boarding/exiting/loading still reset it. The legacy superpower cheat
continues to use its original boost behavior without stacking multipliers.

Implementation uses Slate drawing and the bundled engine font, so it adds no
fonts, textures, dependencies or cooked assets. Historical validation receipts and
scratch backups retain their original identifiers and file paths.

Validation: Editor, Shipping and Development builds succeeded. All 24 focused
headless checks passed without test warnings; both offscreen cases passed after
correcting rounded-shape geometry and strengthening lamp contrast. Inspected
keyboard/controller ON/OFF renders at map scales 1.5 and 2.5, plus expanded help
including the on-foot keyboard layout. Pixel checks confirm that changing input
devices changes only the help strip, leaving the map/lamp/switch identical. Map
click tests pass at scales 1.0, 1.768292 and 2.5. No interactive gameplay or physical
controller test was performed. See [validation results](validation/TurboHudResults.json).

The offscreen runner reports the existing DLSS-G hardware-scheduling startup
messages, also present in the earlier radio render logs. Neither final test case
reported warnings or errors. The first sandboxed test startup could not initialize
Zen cache and was stopped; the normal-cache runner completed successfully.
