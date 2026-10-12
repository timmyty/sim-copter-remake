# Original pedestrian behavior audit — October 12, 2026

The remake runs the original `x/people.df` behavior programs. Several comic interactions are connected, but the existence of a script or animation alone does not prove that gameplay reaches it. This audit compared the supplied original data and `.ghidra-exports` with current runtime callers; it was not an interactive retail/remake comparison.

| Behavior | Evidence and current status |
| --- | --- |
| Notice the player and wave | Connected. Ambient BHAV 600 calls 260: a 1-in-12 roll, same player tile, face the player, `Wave`, wait 15 ticks, restore facing. The VM's opcode 22 and `TryGetPlayerTileProbe` are implemented. |
| React to a nearby helicopter | Connected for statue/plaza people: BHAV 1202 queries the player's helicopter within two tiles and plays `WvNo`. The object-class-2 query can find an unoccupied helicopter while the pilot is on foot. It does not require the helicopter to be parked. |
| Walk up to a helicopter and work on it | Connected for mechanics: BHAV 1204 calls 1020, which approaches the airframe, animates, speaks and advances the work counter. Repair/refuel/rearm spawn these workers through `SpawnCheckupCrew`. This is service behavior, not a general civilian inspection routine. |
| Street conversations, dance and comic voices | Connected. Movement contact invokes BHAV 914/opcode 80 and `RunMeetSelector`; `2Gab`/`HipH` and voice events 10, 1, 4, 11, 2, 18, 6, 7, 3 match `FUN_004c6970`. Ambient BHAV 267 also randomly dances. |
| Watch a fire, approach from farther away, flee when too close | Connected. BHAV 274 calls the implemented fire query, watches at four–five tiles and flees below four. |
| Gawk at a casualty/body | **Restored.** Ambient BHAV 600 calls 273 and its class-15 lookup now finds visible, uncarried casualties/bodies outside hospital tiles. The incomplete export was verified against the supplied Classics executable at `0x004d1c65`, equivalent to the old `0x004caee5`. It accepts written-off people OR medical state 6, unlike the treatment search. Gawkers retain pickup space and abandon a body once carried. |
| Statue antics | Connected through `TrySpawnSpecialBuildingPeople`: building DB selects BHAV 1202, matching original `FUN_004c25b0`. |
| Baseball players | **Corrected after the audit.** Having callers for BHAV 1205/1206 did not prove full teams: budget truncation, passerby suppression, and civilian scan/cap gates produced missing players. Nearby fields now restore the original batter plus seven fielders, class 4 and authored positions/colors; the baseball movement flag also matches `FUN_004c7090`. See [baseball restoration](BaseballTeamRestoration.md) for Canyon reproduction, proximity limits, and validation. |
| Rare character/voice quirks | Class-based voices and rare Elvis voice substitutions are present in `ChooseVoiceSetForBehaviorClass`, citing `FUN_004c71c0`. Visual/listening parity was not checked here. |
| Park and rooftop-worker helicopter waves | BHAVs 1203 and 1201 exist and contain helicopter-facing/waving routines, but no live remake spawn assignment was found. **Not established as a missing retail feature:** the original special-building spawner `FUN_004c25b0` also has no D5 park/1203 or 1201 branch. Find an actual retail caller before calling these restorations. |

## The parked-helicopter question

**Implemented after the audit at the user’s request:** up to three nearby ordinary Sims now walk to a clear spot near the current parked helicopter, gesture, speak using the original nine conversation voice events, and turn toward one another for replies. Visits last eight seconds after arrival; personal cooldowns, clear door routes, responder/pilot yielding and movement/hazard cancellation prevent a persistent surrounding crowd. This gathering is a documented extension using original behavior assets.

Nearby people noticing/waving at the helicopter was already partly present. I did not establish a generic civilian “walk over and inspect any parked helicopter” routine in the supplied retail data. The clearest approach/work routine is the mechanic's. Waving at a helicopter and walking over to inspect it should not be reported as interchangeable behaviors.

**Multi-aircraft mismatch fixed:** `ResolvePlayerHelicopter` now uses the parked/owner-aware `SimCopterHelicopterParking::ResolveCurrentAircraft`. NPCs keep targeting the aircraft the pilot actually exited, matching the original identity at `DAT_005040d0+0xa4`. The shared resolver also excludes support, hidden and destroying aircraft from its fallback. A deliberately possessed/selected fixed-wing aircraft retains its identity for existing passenger systems; it does not attract this helicopter gathering.

## Restoration recommendation and limits

Casualty gawking and the active-aircraft selection are implemented. Further parity work still needs an actual retail caller for the data-only park/rooftop-worker scripts. Generic civilian inspection is implemented as the requested extension, not asserted as proven retail behavior. The old 12.5 Hz versus remake 15 Hz global people timing difference also remains a separate broad compatibility task.

Every primitive opcode used by the shipped BHAV entries has a matching numeric VM case in a static inventory. That does **not** prove every query subtype or spawn path is implemented, as class 15 demonstrates. `Yumm` exists as art but has no shipped BHAV binding; the kiss is represented by voice assets/event 39, not a distinct proven kiss animation. These should not be promised merely because assets exist.

Audit evidence: `Docs/scratchpad/police-roof-patient/behavior-inventory.json`, `original-ambient.txt`, `original-curiosity.txt`, `original-move-reactions.c`; original exports `004c25b0`, `004c6970`, `004c71c0`, `004cac70`. Existing focused behavior/interaction tests passed in the 51-case combined run. Implementation and updated validation are recorded in [Ambient behavior restoration](AmbientBehaviorRestoration.md). No interactive gameplay or listening comparison was performed.
