# Pilot shoulder camera and taser aim

October 10, 2026

The on-foot camera stays behind the pilot's right shoulder. Holding right mouse
button (or the controller's left trigger) blends the camera closer, frames the
upper torso, and raises the right arm. The taser grip follows the animated palm
and the barrel follows the camera pitch. Releasing aim lowers the arm, stows the
taser, and restores the wider shoulder view and the player's configured FOV.

Previously, aiming only changed FOV. The taser was attached at a fixed capsule
offset, while the pilot continued the unmodified idle/walk animation. The centered
camera therefore looked through the body toward a weapon beside the torso.

This is an intentional remake presentation change. The original `privanim.df`
pilot uses right-arm parts `Ne13`, `Ne14`, `Ne15`, `Ne42`, and `Ne44`. The overlay
blends those parts toward the idle arm, rotates them about the shoulder, and
retains the original torso, head, and walking legs. Shared original animation data
is never modified. Only the current posed frame is rebuilt, and only when its
frame, aim blend, or pitch changes; ordinary population animation keeps its
prebuilt sections.

Camera collision uses the spring arm's socket offset, so the sweep includes the
right-shoulder displacement. Normal distance is 130 cm; aiming distance is 60 cm
in the game's quarter-scale world. Both views use an 18 cm right offset. The aim
focus rises from 10 to 12 cm above the capsule center. Parachuting retains its
235 cm wider view and cancels aiming. Carrying, incapacitation, and loss of
possession also cancel aim. No assets, packages, or save-format changes are needed.

## Verification

`SimCopter.RescuePilot.ShoulderAim` exercises real pilot geometry, smooth aim
transitions, grip attachment, camera pitch limits, walking, water displacement,
turning, wall collision, aim release, parachuting, and possession loss. With a GPU
it also captures the actual normal/aim cameras offscreen and compares the aim
image with the taser hidden to verify visible weapon pixels.

Local build logs, automation reports, and captured images are kept in
`Docs/scratchpad/shoulder-aim/`.

Editor and Shipping builds succeeded through the required wrapper. All 14
selected headless tests passed without warnings, including pilot/rescue, taser
custody, original animation parsing, figure adjustments, controller contexts,
lying poses, save round trips, and gameplay controls. The additional GPU run of
the aiming test passed and measured 1,931 visible taser pixels at 1280x720.
The inspected captures establish the pose, upper-torso framing, and weapon
visibility; the fixture uses checker shading and does not establish final city
lighting/material appearance. No interactive gameplay test was performed.

The Shipping executable and symbols were installed into the existing local
playable build with SHA-256 verification and backed-up binaries/manifests.
Source and documentation were synchronized to the local publication checkout;
no commit or push was made. See `Docs/validation/ShoulderAimResults.json` and the
local `Docs/scratchpad/shoulder-aim/installation.json` receipt.

For the final manual gameplay check: exit a helicopter, walk and turn, hold RMB,
look up/down and strafe, fire at a suspect, then release RMB. Confirm the right
shoulder remains the default view, the upper torso and raised taser are visible
while aiming, and the camera moves forward safely beside walls.
