# Cheat codes

During a city game, press **Ctrl + Alt + X**. Type a code and press **Enter** or click **OK** to apply it and return to the game. **Esc** or **Cancel** closes the dialog without applying it. The game pauses while the entry box is open. Close the hangar, settings, or replay panel before opening it.

Codes are case-sensitive, including punctuation. Enter a toggle again to turn it off. Cheats carry over when advancing or warping through a career; starting a new game or loading a save clears the switches. Cheats are not written into save files, but their consequences (cash, aircraft, score, demolition) are part of the normal saved game state.

| Code | Effect |
| --- | --- |
| `superpowermultiply` | Hold Shift for helicopter turbo. On foot, hold Shift to become a fast dog; release it to return to the pilot. |
| `Shields up` | Prevents aircraft collision and fire damage. Does not repair damage already taken. |
| `Gas does grow on trees` | Refills the current aircraft and prevents fuel consumption. |
| `I'm the CEO of McDonnell Douglas` | After entering this code, open the **helicopter catalog in the hangar**, then **press a number key from 1–9 to unlock and deliver that helicopter for free**. Entering the code alone does not unlock a helicopter. See the number-to-aircraft table below; a clear hangar pad is required. |
| `There's no place like home` | Teleports the on-foot pilot to the hangar, leaving the aircraft behind. |
| `I love my helicopter` | Returns the on-foot pilot to their helicopter. |
| `Been there, done that` | Sets a career city's score to its completion target. Return to the hangar to advance normally. |
| `The map, please` | Toggles the map while on foot. |
| `Warp me to career: 12` | Travels to career level 12. Accepts levels **1–30**, retaining cash, equipment, and fleet ownership. |
| `Give me bucks or give me death: 1000` | Gambles for the requested cash. Accepts **1–49999**. A loss ends the current game and returns to the main menu; existing save files are retained. |
| `A megaphone in the hand is worth two in the bush` | Enables on-foot megaphone commands using **F6–F10**. |
| `Out on a Sunday drive` | Toggles a camera riding a city vehicle. |
| `PAMCAREYGOLDMAN` | Reveals the original hidden portrait faces on applicable city signs. |
| `Radioactivity` | Nuclear flash and sound; destroys roughly three quarters of eligible buildings. Roads, airport, hospital, police and fire services survive. |
| `Stop and ask for directions` | Writes the city building grid to `Saved/dump_bm.txt`. The HUD reports its full path. |
| `Gort` | Plays the original alien conversation: fading artwork, synchronized subtitles and left/right voices. **Esc**, controller **B**, or **Return to game** stops it and resumes play at any time. It also returns automatically after the full **3:06** sequence. |
| `HSI` | Toggles **FuzeTheory and Invoke the Revoked - Humanist Superintelligence (HSI) - Official Music Video plays** (`HSI.mp4`). |
| `Play video: My Movie.mp4` | Plays a file from the documented `DriveInVideos` folder. |
| `Stop video` | Stops the drive-in picture and sound. |
| `Lights, Camera, Action!` | Toggles the supplied original movie on the city's drive-in screens. See [video setup](../README.md#cheat-codes-and-drive-in-videos). |

After entering `I'm the CEO of McDonnell Douglas`, use these number keys while the hangar's helicopter catalog is open to choose which helicopter to unlock:

| Key | Aircraft |
| --- | --- |
| 1 | Bell Jet Ranger |
| 2 | MD 500 |
| 3 | Apache |
| 4 | Bell 212 |
| 5 | Schweizer 300 |
| 6 | Agusta |
| 7 | Dauphin |
| 8 | MD Explorer |
| 9 | MD 520 |

Delivery adds a separate aircraft without charging cash or changing the currently selected aircraft. Already-owned types cannot be duplicated.

## Fidelity and known limits

The entry route and gameplay switches were previously absent. This implementation ports the original command matching and principal effects into the remake's existing UI, flight, career, and hangar systems.

- `FUN_00435680` is the command handler: independent case-sensitive substring matches, repeatable switches, on-foot/career restrictions, two-character career-number parsing and five-character cash parsing. The first eight phrases can be decoded directly from the delta-encoded stack data in the export; the remaining phrases are corroborated by [GameFAQs](https://gamefaqs.gamespot.com/pc/198651-simcopter/cheats) and [Cheat Code Central](https://www.cheatcc.com/articles/simcopter-cheats-codes-cheat-codes-walkthrough-guide-faq-unlockables-for-pc-pc/).
- Flight references: `00484d20`, `00489800`, `00486e90`, `00487160`. Pilot dog: `004c1b50`; quadruped animation remapping: `004c68f0`. Completion target: `00407b30` / `00407ae0`. Portrait visibility: `0049a940`. Car camera selection: `0049ae70`.
- `Radioactivity` follows `004515d0` / `004a6940` for the flash, blast and building survival rules. The original's one-in-32 class-1 smoke scatter is implemented. Those plots also display small cosmetic flames for 30 seconds, fading over the last three; these do not create missions. Additional NPC removal and mission termination are not yet ported.
- `Gort` preserves `00445800`'s 36 subtitle/voice pairs, including its nonsequential voice choices, repeated speakers, four-second main-dialogue spacing and six-second bonus-dialogue spacing. The first subtitle begins at 0:04; after "Trust me..." at 1:20, the pause is shortened from the original 150 seconds to **10 seconds**, so bonus dialogue begins at 1:30. The full sequence ends at 3:06 after allowing six seconds for its last line. `00446a20` fades in the still `martian.bmp` and draws subtitles in the left/right rectangles from `004455e0`; the original renderer has no moving mouth frames. Original mono samples are preserved and routed to the speaker's stereo channel, following `00446af0`. Remake conveniences are the requested shorter pause, a smooth half-second fade, immediate Esc/B/button cancellation, and automatic completion returning to the same game and pawn. See [implementation and validation](memory/simcopter-gort-sequence.md).
- `Stop and ask for directions` follows `00495700`'s file-export intent; the remake exports hexadecimal building IDs rather than the original's full annotated ASCII report.
- `Lights, Camera, Action!` loads `movie*.smk` via `004477b0`. The remake uses the supplied video and an HD MP4 player on the authored CO182 movie face (object 7, page 2/cell 1). `HSI`, `Play video: filename.mp4`, and `Stop video` are explicit remake extensions. Custom movie commands consume the full line so filenames cannot accidentally trigger other cheats.
- The money threshold is the original `(amount - 50000) / 2 + 50000`. The export omits the RNG call argument; the remake uses a 0–49999 roll. Exact odds need the original call-site disassembly to verify.
- The original entry UI is restored: `MBox.bmp`, `MBoxCht.bmp`, `MBoxl.bmp`, `button.bmp`, the exact prompt, original control positions, append/Backspace typing, a 700 ms blinking underscore, and 127 typed characters plus the caret. Ctrl+V pasting is a remake convenience. Artwork scales with the original aspect ratio; fonts use the remake's shared Slate rendering. Enter dismisses the dialog before applying the code; feedback appears on the HUD. References: `004354c0`, `004426c0`, `004428f0`, `00441f60`, `0043d0c0`, `0044bf70`.
- The remake uses immediate career travel, and a return to the main menu for a lost gamble. Its cheat switches reset per new/loaded game rather than only at process startup. No unsupported “Make me big” or Comanche sequence was invented.

## Original cheat implementation validation (before the drive-in update)

Editor and Shipping builds succeeded. All **62 targeted automation tests passed**, including seven new cheat tests covering original parsing, dog assets, fuel/shields/turbo, free aircraft delivery, state lifetime, dialog cancellation, portrait visibility and actual city demolition. Three test cases emitted the existing synthetic-world foliage attachment or missing world-context warnings; none failed.

The validated Shipping EXE/PDB are installed in `C:\SimCopterRemake\SimCopter Remake v1.0.1\Windows`. Both replaced files and the package manifests were backed up and hash-verified in `Docs/scratchpad/cheat-codes/before-install-20261008-235344/`. Saved games, user cities and music were not installation targets. The complete test report, binary hashes and installation record are alongside that backup.

This change is C++ only and reuses already packaged original data and materials. No live foreground gameplay verification was performed.

See [drive-in/UFO implementation notes](memory/simcopter-drive-in-ufo.md) for the subsequent media, aftermath and original-art update.
