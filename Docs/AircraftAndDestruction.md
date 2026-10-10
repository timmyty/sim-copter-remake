# Airplane controls, collision reactions, and Apache destruction

October 10, 2026.

- The Transport airplane uses **W/S** (or up/down arrows) to climb and descend. **Space/Ctrl** increases/decreases a retained throttle setting independently of pitch. Hold Space to start; reducing throttle while stopped eventually shuts the engine down. Controller pitch and collective follow the same separation.
- The airplane dashboard adds a metal control plate beside the existing gauges: a **GEAR UP/DOWN** knob and **SPEED** slider with a percentage. **G** also toggles gear. Gear cannot retract on the ground; a gear-up landing damages the airplane and stops it. Throttle and gear state are saved; older aircraft saves default to gear down.
- The original PLANE1 propeller blades, spinner, blur discs, and landing gear are separated from the fuselage. The propeller spins during powered operation or motion, including the paused city-introduction flyovers. Flyovers retract their gear. No replacement airplane art is required.
- Walking into a fixed obstacle plays the shipped `bowang.wav` spring cue. Walking into an NPC triggers the original neutral-person conversation reaction, with a randomized Simlish reply from the pilot. Contact audio is rate limited; ground contact does not trigger it.
- Apache missiles immediately explode cars. The first machine-gun hit ignites and stops a car with a two-second fuse; another hit explodes it immediately. Both paths release vehicle collision and settle existing traffic/fire missions. Water can still save a burning car before it explodes.
- Both Apache weapons injure pedestrians using the medical system. Existing injured patients are retained, and mission-pool exhaustion does not make a struck pedestrian immune.
- A direct missile hit collapses the entire building footprint into the existing rubble/debris effect, clears collision and traffic building data, and settles fires as burned out rather than rewarded extinguishments. Missile destruction includes service buildings; the Radioactivity building-exemption rules remain separate. Demolished buildings use the existing saved demolition list.
- **Radioactivity** injures a shuffled 65% of eligible healthy walkers, retaining at least one survivor per use. Existing patients, passengers, emergency crews, cows, and people in custody are excluded. Injuries use safe outdoor placement and can be rescued; cheat-caused medical calls cannot produce rewards. Repeated uses can affect previous survivors.

Original references: `FUN_004c9470` (person contact), `FUN_004c6970` (conversation selection), `FUN_004c5210` (voice bank), `FUN_0049f680` (vehicle weapon reactions), `FUN_004515d0` (nuclear population pass), `FUN_004a6940` (building selection), and `FUN_004a5fd0` (rubble/collapse). Retained throttle, retractable gear, immediate full-footprint missile destruction, and the 65% injury rule are intentional remake behavior.

Validation evidence is stored in `Docs/scratchpad/aircraft-destruction/`. The automated checks exercise projectile collisions, population injuries/survivors, whole-city demolition, controls, save migration, propeller poses, and the cockpit widget. See the final validation record for actual results.

Editor and Shipping builds succeeded; 123 headless tests and both offscreen gear variants passed. Installed with verified backup and hashes. [Detailed results](validation/AircraftAndDestructionResults.json). No interactive gameplay or live listening check.
