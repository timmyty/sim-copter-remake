# Radio and medic update — October 14, 2026

The medic early-return exception described below is superseded by the October 15
[crew cooldown update](CivicAndWaterfrontReview.md): manually ejected police and
medics must wait the full 15 gameplay seconds, including after a final patient handoff.
The remainder of this document records the earlier implementation and validation.

Radio uses the original category schedule by default on retail stations. Track
selection no longer silently creates a sequential queue. The original constructor
`FUN_0042e570` appends each of music, DJ, commercials and jingles once; the remake's
previous four music entries delayed ads. `FUN_0042f160` supplies the existing
probabilities and gap, while `FUN_00430070` avoids repeating an interruption at
the cycle seam. Retail category lists retain the shuffle from loading and wrap
as in `FUN_0042f3d0`. Original decompiles are in `.ghidra-exports/`.

The Radio Player's order label is now a button. Shuffle is the default except for
KINV, which retains its album order and saved song/position. Optional collections
shuffle their music only. Order choices are retained per station for the current
world, with no profile or save-format change. Existing explicit Sound settings
for commercials and DJ remain respected. The menu and uninterrupted playback work
on foot, including pause, seeking and track selection. Front-end and city-tour
audio gates remain separate from helicopter possession.

Scheduled MedEvac parties already use current-aircraft passenger seats minus one
in both career and user cities. Incident-created one-patient missions now also
refuse creation when the aircraft cannot seat a patient plus a medic. Healthy
transport/rescue groups retain their existing capacity rules. Existing physical
injuries and restored missions are not deleted or resized.

Manually deploying a medic starts a 15-second simulation-time boarding delay.
The only early return is with their final living, undelivered patient and space
for both. Nearby waiting patients and teammates' carried patients prevent that
exception. The exact patient handoff is remembered because retail BHAV 275 puts
the patient into the helicopter before BHAV 269 boards the medic. A different
medic cannot borrow that exception. The normal teammate seat reservation remains
in force after the timer expires. The short deployment timer is transient and
is not added to the existing save format.

Validation artifacts and source baseline are retained under
`Docs/scratchpad/radio-medic-update/`. Final results are recorded in
`Docs/validation/RadioAndMedicUpdateResults.json`.

Editor, Shipping and Development builds succeeded. The broad focused run passed
all 99 cases (96 clean, three with pre-existing synthetic-world/background-network
warnings). After the final shuffle-navigation change, all 13 radio cases passed
again with offscreen rendering, including real controller input processing for the
on-foot shortcut. The 4:3 and 16:9 Radio Player images were inspected.

Coverage includes repeated advertisements, original category counts, both playback
modes, KINV's saved progress and complete shuffle bag, previous/next navigation,
on-foot transport controls, menu pause/input restoration, capacity across seats,
difficulty tiers and seeds, actual helicopter changes and on-foot capacity, manual
medic deployment, the 15-second boundary, carried and completed final handoffs,
other waiting patients, insufficient seats, and repeated deployment.

No interactive gameplay, physical-controller or listening check was performed.
Installation status and hashes are recorded separately in the validation receipt.
