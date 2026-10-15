# Nearby baseball games — October 12, 2026

The user's final choice is two **nine-player rosters**, with actors only for players currently on the field. This supersedes the intermediate eight-person small-team proposal. This is an explicitly requested remake extension, not a claim about retail SimCopter: the decoded original BHAVs 1205/1206 only perform individual gestures and base-running pantomimes.

## Game and presentation

Each active diamond has nine defenders (pitcher, catcher, four infielders, three outfielders), the current batter, and any base runners. The batting order contains nine identities per team. Benched identities have no actors, meshes, collision components, or behavior ticks. The active count is **10–13**, never eighteen.

Games play nine innings with three outs per half inning, separate batting orders, ball/strike counts, four-ball forced walks, strikeouts, two-strike foul protection, singles/doubles/triples/home runs, groundouts and caught fly outs. Ties continue into extra innings; a home lead can skip the bottom of the ninth, and walk-offs end the game immediately. The final score remains briefly before a new game begins. A field display shows runs, inning, count, outs, and the current result.

The coordinator selects scripted pitch/contact outcomes and directs visible pitches, bat swings, arcing hits, retrieval, return throws, and runners following each base in order. The figures reuse the original class-4 meshes and animation clips, with separate uniform colors. Bat and ball geometry is generated in code using an existing material. This is ambient baseball rather than a complete competitive sports simulator: no steals, bunts, double plays, errors, substitutions, or tag-up decisions. Groundouts target first; runners hold on caught flies and batter outs. Ordinary hits conservatively advance existing runners by the hit's base count.

### Distinct uniforms and scoreboard follow-up

Visitors now wear **red jerseys** and the home team **blue jerseys**, retaining their team colors when batting, running, or fielding. The original `5man` jersey parts use recolorable ramp 1: clothes offsets 3 and 6 select original palette ramps 4 (red) and 7 (blue). The previous visitor offset 1 selected another blue ramp, so the initial team distinction was too subtle. Original figure meshes, faces, animation, and fixed-color parts are preserved.

The initial presentation follow-up used a dark-backed billboard with team labels and a final winner banner. The user's subsequent stadium-photo reference explicitly supersedes that floating presentation; the physical scoreboard below is the current design. The final result still remains for fifteen seconds before the next game clears the totals and winner.

### Fixed stadium scoreboard

The board is now an upright **stadium structure fixed toward home plate**, with a deep green metal cabinet, rear ribs, rain hood, faded cream trim, steel support posts, cross-bracing, and concrete footings traced to the actual field surface. It has front, side, and rear geometry and casts a world shadow. Moving the helicopter does not rotate or tilt it.

Painted 5×7 block lettering identifies `SIMCOPTER PARK`, visitors, home, inning, ball, strike, and out. Red and blue painted markers retain the uniform association. Amber incandescent-style dot matrices show scores, the current inning and half, pitch counts, and the current play. The lower matrix reads `FINAL - VISITORS WIN` or `FINAL - HOME WIN` when the game ends. Three-digit totals or extra innings fit their allotted displays automatically.

Cabinet and socket geometry are built once per active field; a changed readout rebuilds only its own lamp mesh section. The bulbs use the existing vertex-color material with the shared exposure compensation, with no individual light actors, textures, new cooked assets, additional NPCs, or physics bodies. The whole structure follows the game's existing nearby-field lifetime. This is a requested visual extension inspired by the user's photographs, not a claim that retail SimCopter had this scoreboard.

## Lifetime, safety, and performance

- A field activates within eight city tiles horizontally of the controlled pawn (helicopter while flying, pilot on foot); it retires beyond twelve tiles. At the usual tile scale these are 32 and 48 metres. The gap prevents rapid boundary respawns.
- At most two nearby games are active, for at most 26 baseball actors across both fields. Existing population caps also apply. A new game requires capacity for thirteen; later capacity shortages pause roster completion and retry without duplication.
- One coordinator per field makes decisions at 15 Hz. Normal agent ticks move and animate only active players. Ball/bat meshes are built once per game; score text changes only when the displayed state changes. No new navigation system, physics ball, cooked asset, or plugin is introduced.
- The original VM remains intact under transient baseball control. Injury, carrying, knockdown, or mission ownership take priority. An interrupted diamond waits for its patient to leave; cleanup does not delete that patient. Blocked runner movement cannot silently award runs and eventually replays the pitch.
- Healthy legacy eight-player teams are migrated by exact field identity. The legacy restoration and its tests remain available through `bPlayFullBaseballGames=false`; full games are the default.
- Games are ambient and transient: leaving their activation area or loading a save starts a fresh game later. Healthy game actors are excluded from traffic saves; injured or mission-owned players remain serialized normally. No save format change is required.

These bounds reduce avoidable work but are **not an FPS guarantee**. No live frame-time benchmark was performed.

## Validation

The combined headless report contains 40 successful cases, with no failures or warning-bearing cases: 39 exercised logic/regression cases and the explicit NullRHI skip of the capture case. The capture case is separately run with D3D12 offscreen rendering. Coverage includes:

- Counts, forced and unforced walks, all hit distances, nine-person batting-order wrap, three-out changes, extra innings, skipped bottom ninth, walk-offs, and immutable final scores.
- Actual actor movement through complete nine-inning games, reaching 8–6 with the fixed test seed, on a flat fixture **and all four actual Canyon fields**.
- Maximum thirteen active actors with loaded bases, no actors for benched players, migration of old teams, partial-capacity retry, removal of a field, injury/save restoration, and blocked plays.
- Possessed-helicopter approach, stationary observation, hysteresis, departure cleanup, and return.
- The previous eight-player retail behavior/roster tests plus existing behavior, people, city population rules, and save tests.

The offscreen capture was inspected. It found and corrected an initial bat at the field origin and inward-wound prop faces. Geometry assertions now verify clockwise outward-facing ball/bat triangles. The capture uses the real Canyon stadium and shows the defensive lineup, batter/catcher, pitch, bat, and score display. It is not an interactive playtest; no audio or live performance claim is made.

Editor, Shipping, and Development builds and the final installation receipt are recorded in [BaseballGamesResults.json](validation/BaseballGamesResults.json). Source baselines, diff, whole-module source hashes, test reports, screenshots, build logs, and installation backups are retained in `Docs/scratchpad/baseball-games/`. The separate publication checkout is unchanged.

The uniforms/scoreboard follow-up passed Editor, Shipping, and Development builds, 41 exercised headless logic/regression cases (42 reported successes including the NullRHI capture skip), and the separate D3D12 capture case with no failures or warning-bearing cases. The added tests inspect actual visible jersey vertex colors through loaded bases and a side change, and verify score/count updates, visitor wins, home walk-offs, retained final scores, and new-game reset. Both live and final Canyon captures were visually inspected; the final image uses a staged 7–4 result to inspect the winner display. Both installed launcher executables and symbols match the new builds, with verified backups. See [BaseballPresentationResults.json](validation/BaseballPresentationResults.json) and `Docs/scratchpad/baseball-presentation/`. No interactive playtest or live FPS benchmark was performed.

The final stadium-structure revision passed all three builds and the same 41 exercised headless cases plus the separate D3D12 capture case. Scoreboard checks now verify grounded supports, cabinet depth, an upright home-plate orientation that stays fixed as the focus moves, and actual lamp-geometry changes when the score changes. Final [live](screenshots/baseball-stadium-v1.0.9.webp), [winner](screenshots/baseball-final-v1.0.9.webp), [side](screenshots/baseball-side-v1.0.9.webp), and [rear](screenshots/baseball-back-v1.0.9.webp) renders were inspected. The final score is staged for display inspection; the rear uses a rotated key light because the isolated fixture has no sky fill. Both installed executables and symbols match the rebuilt files, with verified backups. Evidence is in [BaseballStadiumScoreboardResults.json](validation/BaseballStadiumScoreboardResults.json) and `Docs/scratchpad/baseball-stadium-board/`. No interactive or live performance claim is made.
