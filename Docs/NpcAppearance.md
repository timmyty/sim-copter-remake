# Matching NPC identities and medic care portraits

The passenger window, helicopter cabin and walking population share each person's resolved
original figure, clothing palette and head. Cabin occupants use the original `privanim.df`
body parts in a seated pose and the same `SIM3D` head texture used on foot. The pilot uses the
original pilot model/head in all ten flyable aircraft. Vacated seats remove their occupants.

Separate hair and helmet parts in `Blonde`, `2woman`, `2blonde` and `Fireman` now follow their
portrait identity: golden blonde hair, auburn hair, black hair and a red helmet respectively.
In particular, the black-haired character's hair no longer changes with the clothing offset.
These named ARCP part color corrections are shared by the walking and cabin renderer.

The original `PEOPLE1.BMP` portraits retain their faces, hair, hats and three expression rows;
only the shirt palette follows the actual figure's clothes. Appearance cache keys include the
person, resolved body, head and clothes. Saved people now record a resolved body even when it
was selected from a population pool, and restoration rebinds the saved head texture after
restoring the behavior state. The existing save format remains compatible.

The portrait palette is quantized to multiples of four while GEO's palette is full eight-bit
RGB. Matching accounts for that difference before recoloring, preserving the portrait shading.

## Original reactions and medical benefit

BHAV 264 still controls flight and patient expressions. Ordinary passengers use row 1 at
damage-adjusted speed 0–125, row 0 at 126–250, and row 2 above 250. Impact flinches and recovery,
patient health expressions, injury damage, voices and health-dependent EKG pitch remain under
the existing behavior code. Rendering does not write moods or health.

The original BHAV 281 already gives a patient aboard the player's helicopter a 1-in-20
deterioration roll when opcode 73 finds a hidden state-5 medic, versus 1-in-10 without one.
Each successful roll drains `1 + difficultyTier`; the benefit roughly doubles expected survival
time rather than healing or making patients immortal. The original hidden-medic lookup is
map-wide (a medic in an ambulance can also qualify), and that rule is unchanged.
See [the original behavior trace](memory/simcopter-paramedic-handoffs.md#a-medic-in-your-cabin-halves-the-patients-deterioration-bhav-281).

## Requested medic animation

A medic sharing the cabin with at least one living medevac patient displays moving sleeves
and gloved hands over a patient blanket in the portrait. This is a new presentation addition,
not animation claimed to exist in the original game. It uses cached pixel-art frames while
retaining the same identity and current flight-reaction face. A tooltip explains the benefit.

The display reads the current manifest and signed patient health, so it stops when the last
living patient leaves, dies, or is written off, or the medic leaves. It continues while another
living patient remains. Animation follows world time and freezes when gameplay is paused.
The animation cannot alter treatment probability or patient health.

## Validation

`SimCopter.Appearance.IdentityAcrossViews` compares actual walking/cabin mesh colors and head
pixels across human behavior classes and clothing variants, portrait faces across all moods,
save/load identity, all ten aircraft pilots, cabin cleanup, original impact reactions, and
the medic animation's lifetime. `SimCopter.Appearance.RenderedCharacters` exports offscreen
walking/seated comparisons and checks visibility through an actual canopy. Existing passenger,
medical, rescue, save and aircraft tests cover related behavior.

Build/test results and deployment evidence are recorded in `Docs/validation/NpcAppearanceResults.json`.
Scratch logs, previews, source backups and installation receipts are under
`Docs/scratchpad/npc-appearance/`. No interactive gameplay check is performed.
