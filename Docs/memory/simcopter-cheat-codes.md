# Original cheat entry and effects

Implementation and player guide: [CheatCodes.md](../CheatCodes.md). Ground truth: `.ghidra-exports/00435680.json`.

Important integration details:

- Ctrl+Alt+X is intercepted by the existing Slate keyboard preprocessor before X can fire the active tool. Respect game/editor focus, UI-only mode, hangar/replay ownership, and repeated key events. The dialog uses the controller's reference-counted pause and restores pawn-specific input on close.
- State is owned by the career subsystem, retained by `ContinueCareerIntoNextCity`, cleared by `BeginCareer`, and deliberately absent from save serialization.
- Fuel and damage guards are in the fixed-point flight model, with matching guards in the custom plane path. Turbo reuses the existing original-model input. On-foot Shift swaps the actual privanim dog figure, clips and movement parameters.
- Catalog digits address runtime types, not display rows: 3 is Apache, 5 Schweizer. Delivery uses `SpawnOnFreePad`, grants ownership only after success, and never charges money or replaces the current pawn.
- SIM3D page 2 cell 0 is the hidden PAM portrait, previously removed as debug geometry. Its faces now live in initially hidden building parts with full instance/demolition bookkeeping. This corrects the older memory note that said the shipped game never shows these faces.
- The nuclear building predicate is grounded in `004a6940` and preserves essential services. The guide explicitly lists unported NPC/mission cleanup and the uncertain money-roll range. Timed alien dialogue was subsequently restored; see [Gort sequence](simcopter-gort-sequence.md).
- Unreal initially reused a cached makefile that omitted the newly added test source. Refreshing the module Build.cs timestamp rebuilt the makefile and included it; do not accept a test run that reports no matching tests as validation.

Editor/Shipping succeeded; all 62 targeted tests passed, including seven new cheat cases. Three cases emitted existing synthetic-world warnings. Installed EXE/PDB with verified backup and hashes; no live gameplay check. Evidence: `Docs/scratchpad/cheat-codes/`, including `installation.json` and `tests-verified/index.json`.

Two failures caught during implementation were fixed before installation: the full figure name is `2DOGG` (the original compares only its first four bytes), and the pre-existing `bTurbo` implementation only handled forward velocity. Vertical turbo now follows `00487160`, including the original negative-collective sign reversal and rotor/ceiling gates.

The subsequent [drive-in/UFO/Gort update](simcopter-drive-in-ufo.md) adds movies, post-blast visuals, the original UFO, Gort voice clips, and the original bitmap cheat-entry UI. Enter/OK closes the dialog and applies the code; Esc/Cancel cancels. See that note for the current validation and installation.
