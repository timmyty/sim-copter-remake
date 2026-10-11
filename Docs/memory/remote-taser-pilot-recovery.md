# Remote taser and unattended pilot recovery

User-requested remake additions, October 10–11, 2026. Player guide:
[RemoteTaserAndPilotRecovery.md](../RemoteTaserAndPilotRecovery.md).

- Every helicopter has a procedural, gimballed nose taser. `ToggleRemoteTaser`
  replaces the officer view; there is no passenger or purchase gate. The right-side
  AIM TASER / FLY HELICOPTER button is mouse-clickable and cannot take keyboard
  focus. RMB + mouse / right stick aims; LMB, X or RT fires. N / R3+Y switches mode.
- Aim, camera, shot origin and visible gimbal share the real muzzle transform.
  Automatic orbit tracks the live suspect. Aircraft weapon commands remain blocked
  during aiming, including direct Apache requests.
- `SimCopterPilotRecovery.cpp` owns the unattended helicopter state. Two-second
  hold, nine ground probes, whole-aircraft swept descent, landing on flat clear dry
  ground, otherwise low hover. Bought harness (career bit, not debug grant) lifts
  the actually carried patient or robber first and then the pilot. Use the real
  `BoardCarrier` custody/capacity path and the existing rope/harness graphics.
- The winch checks the entire lift corridor. A full cabin or obstruction holds the
  recovery; never destroy a person, invent a seat, or pull them through geometry.
  Stop character movement while attached; refresh rotor animation while lifting.
- Clear all transient flight/start/winch inputs during possession changes. Grounded
  engine startup must not be mistaken for a saved airborne recovery. The cabin
  shows a pilot only when possessed or flown by the support pilot, not during rescue.
- On-foot save version 4 adds the attached-pilot flag and reads versions 1–3. After
  relinking the parked helicopter, recovery resumes from the actual saved rider.
- Ownership checks here use the actual `APlayerController`. UE 5.8's
  `APawn::IsPlayerControlled()` instead requires a non-bot PlayerState, which can
  be absent during synthetic tests and transitional loading.
- Damaging dry falls play original voice event 30; falls >=1800 cm add generated
  bone-crack PCM through independent mixer voices. Soft parachute landings/water
  remain exempt. Turbine ISMs now block on cooked runtime blade triangles, and
  instance rotation updates collision as well as drawing.

Evidence is in `Docs/scratchpad/remote-taser-recovery/`. Focused tests include real
possession changes, patient/robber/solo recovery, save/resume and obstruction,
startup exit, nonlethal custody, moving blades and blade gaps. Offscreen Slate and
scene captures inspect both button states and all nine aircraft mounts.

Shared-workspace build trap: concurrent edits to actor layouts can leave mixed
object layouts in incremental builds. A consistent dependency rebuild removed the
observed prisoner-handoff access violation. Also, Editor startup invokes
Build.bat for SDK validation and waits behind other builds; `-Multiprocess` skips
that redundant SDK subprocess for these isolated test runs. Keep unique logs.

October 11 pursuit/carry/standby follow-up:

- Remote mode selects criminal cars as well as uncaptured robbers, transfers from
  a car to its visible burglar, follows target velocity, and samples clear side or
  elevated vantage points. Actual flight uses the swept autopilot with approach
  descent enabled only for pursuit; ordinary support behavior keeps its old gate.
  Lost targets cause a hover and periodic search; fully occluded targets cause a
  bounded climb, never endless ascent. Manual gimbal offsets remain under player
  control. Direct mouse travel is not multiplied by frame duration; controller
  rates are. RMB temporarily releases the cursor for cockpit buttons.
- `BeginPassengerFall` previously cleared `bMissionCarried` without detaching the
  root, so dropped burglars still followed the pilot. It now detaches, clears the
  carrying tilt and restores visible/collidable state. `LastPutDownPerson` blocks
  automatic repickup until the pilot walks beyond 140 cm and returns; the existing
  drop cooldown alone was insufficient when standing next to the released person.
- Hospital boarding policy allows one standby medic without a medical event.
  Both selection and `BoardCarrier` use the policy, so concurrent approaching crew
  cannot all claim standby seats. Patient unloading remains first, and further
  crew still require a waiting patient. Physical approach/height/capacity gates
  remain. Regression evidence: `Docs/scratchpad/taser-pursuit/`.
