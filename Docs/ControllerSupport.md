# Controller Support

The game uses Xbox-style button names below. Controller input is context-sensitive so the same
buttons can cover flight, camera adjustment, dispatch, tools, and passengers without removing
the existing keyboard and mouse controls.

## Helicopter

| Control | Action |
| --- | --- |
| Left stick | Analog equivalent of `WASD`: forward/back pitch and coordinated left/right turn |
| Right stick | Pan/orbit the camera like holding the right mouse button |
| `RT` / `LT` or `B` | Climb / descend; on the ground these also hold engine start / shutdown |
| Click `R3` | Open Tools; click again for Dispatch, then alternate between the two |
| `X` while a radial is open | Switch Tools / Dispatch |
| `A` | Use the selected tool / confirm a menu choice |
| `LB` / `RB` | Turn left / right |
| `Y` | Exit the helicopter, including deliberate airborne exits |
| View / Back / Select | Cycle camera view; close an open Tools, Dispatch or passenger panel |
| Menu / Start | Pause or resume |
| `L3` | Toggle Turbo Mode |
| `R3` + `L3` | Show / hide contextual button help |
| `X` + left-stick X | Dedicated lateral slide, equivalent to `Q` / `E` |
| `R3` + D-pad up/down | Minimap zoom in/out |
| `R3` + D-pad left/right | Previous/next map mission |
| `L3` + D-pad up | Toggle mission markers |
| `L3` + D-pad down | Toggle service vehicle markers |

Turbo toggles when an ordinary L3 click is released. Using L3 for a marker shortcut
or the help chord leaves Turbo Mode unchanged. Map chords consume the D-pad press
so they do not also operate a winch or change megaphone messages.

LB and RB remain dedicated to turning. Holding both cancels their turn input;
releasing either immediately restores the other direction, including at cruise.
The retired LB+RB tool-wheel shortcut no longer latches steering off. Click R3 for Tools.

A plain Select press advances once through Chase, Orbit, Rescue, Cockpit and Spotlight,
then returns to Chase. Holding the button does not skip views. From the remote-taser
sight it restores the aircraft view and advances the camera. Holding R3 or the camera
adjust modifier while pressing Select retains the deliberate Radio shortcut.

In the hangar Catalog, left/right stays among the aircraft tabs. Press Down to reach
Upgrades or Apache; Up returns to the aircraft row. This applies to the D-pad, stick
navigation and keyboard arrows.

### Camera adjustment

Hold `X` to enter camera-adjust mode:

| Control while `X` is held | Action |
| --- | --- |
| Right-stick Y | Zoom in/out |
| `RT` / `LT` | Move the helicopter up/down in the framing of exterior camera views |
| `A` | Toggle the spotlight |

Flight collective is suppressed while `X` is held so camera framing cannot accidentally change
altitude. Left-stick forward/back remains analog flight input; left/right becomes the lateral
slide described above.

## Dispatch radial

Click `R3` to open Tools, then click `R3` again to switch to Dispatch. Select a service
with the right stick, including ground services and **Air: Map Mission**. Air support
unlocks after passing the current level and waits for an order. Select the mission
on the map, then confirm **Air: Map Mission** to assign that one job. The helper
returns to standby afterward; each further mission requires another order.

| Control in Dispatch | Action |
| --- | --- |
| `B` | Cancel and close without dispatching |
| Select Recall All, then press `A` | Recall every active dispatch and chase vehicle, then close |
| `R3` (or `X`) | Switch back to Tools |
| `A` | Dispatch the highlighted service and close |

The spotlight determines the dispatch and chase tile. Recall All is immediate and does not require
a valid spotlight target; the original Shift+F2-F5 bindings retain their selected-service,
spotlight-local release behavior.

## Tool radial and use

Click `R3` to open the tool radial and navigate with the right stick. The menu stays
open after releasing R3 and while idle. Press `A` to equip the last highlighted
installed tool and close, even if the stick has returned to center. Press `B` to
cancel without changing tools. Another R3 click switches to Dispatch without
equipping or activating the highlighted option. X remains an alternate page switch.

| Control | Action |
| --- | --- |
| `A` | Activate the equipped tool; held tools remain active until `A` is released |
| Select Passengers in Tools, then press `A` | Open passenger selection |
| Select purchased Remote Taser in Tools, then press `A` | Activate the remote sight while airborne |
| D-pad up/down with bucket selected | Raise/lower the bucket |
| D-pad up/down with rescue harness selected | Raise/lower the harness |
| D-pad left/right with megaphone selected | Select the previous/next message |

The radial contains installed tools, the purchased Remote Taser, and Passengers when the cabin is occupied. In remote mode, right stick aims, hold LT for 4x scope zoom, and A or RT fires. Release LT to return to the previous zoom. The mouse wheel adjusts zoom from 1x to 4x. The helicopter holds a steady firing position until range or sustained cover requires following again. Click R3, select Fly Helicopter, and press A to return. Selecting an ordinary tool also returns to manual flight. Confirming an option does not perform a tool action on the same press.

## Passengers

Click `R3`, select Passengers with the right stick, then press `A`. The selected
seat is highlighted on the dashboard.

| Control in passenger-selection mode | Action |
| --- | --- |
| D-pad left/right | Select a passenger |
| `A` | Open the Drop / Cancel confirmation |
| D-pad left/up or right/down | Select Drop or Cancel |
| `A` | Confirm the highlighted choice |
| `B` | Back out one level; press again to leave passenger mode |
| Click `R3` | Return to Tools |

## On foot

| Control | Action |
| --- | --- |
| Left stick | Move |
| Right stick | Look |
| `LT` / `RT` | Aim / fire the taser |
| `L3` | Toggle Turbo Mode |
| `R3` + `L3` | Show / hide contextual button help |
| `A` | Jump |
| `X` | Put down a carried mission person |
| `Y` | Enter a nearby helicopter |
| `B` | Toggle parachute while falling |
| Menu / Start | Pause or resume |

Mission-person pickup remains proximity-driven, as in the existing on-foot mission flow.

## Menus

The front end, city and save pickers, pause/settings pages, confirmation dialogs,
hangar pages and airport check-up use spatial controller navigation:

- Left stick or D-pad moves the gold outline toward the control in that direction,
  using its on-screen position. Disabled controls are skipped; focus stops at the edge.
  Outlines follow the visible artwork: the main and pause menus use their recessed
  label bezels, city previews use rounded openings, and dialog buttons follow their
  frames. These visual bounds scale with the page without changing mouse hit regions.
- `A` activates the outlined button, hotspot or checkbox.
- On a slider, list, dropdown or text field, `A` enters the control. The outline turns
  green while adjusting. Use the stick or D-pad to adjust sliders or select list entries,
  then `A` to confirm. Text entry itself uses the platform's text input.
- `B` first leaves an active control, then cancels or goes back one page. On the root
  main menu it stays on the menu; choose Quit and press `A` to exit.
- Hangar catalog tabs and equipment hotspots are included. Settings scroll to reveal
  the selected control. The original main-menu lamps and city preview selection follow
  controller navigation.
- In Helicopter Equipment, one A press on an item moves to Buy, Sell, or Done,
  according to availability. A fresh A press activates that button. B returns to
  the selected item before leaving the page. LB/RB browses helicopter previews.
  A mouse click keeps the item selected while the pointer crosses other rows.

Menus show the selection outline without a button-help text strip. Keyboard and mouse
retain their existing controls. The check-up panel owns UI input while open and returns it
to flight when closed.

## Wheel layout and input contexts

The wheels use a compact segmented ring with a clear selected rim and a central title.
They scale down to fit small viewports. Slot zero is above the hub and slots proceed
clockwise. Painting and selection share `GetRadialSlotDirection`; Unreal's viewport converts physical right-stick Y
to downward Y before the pawn receives it, so selection uses that value directly. The former constraint-canvas labels
were offset twice because slots default to centre alignment, so the visible labels
were displaced from their selection directions.

Police (Chase) is a separate fourth segment, using the police service in spotlight-chase mode.
Both wheels commit on `A` only with a highlighted segment. The wheel fades out over
0.25 seconds; the activated segment turns gold and fades over 0.75 seconds. Actions take effect
immediately, and the fading wheel never captures input. Centring the stick retains
the highlighted choice so A can confirm after releasing the stick. A newly opened
wheel starts unselected; pointing elsewhere changes the choice. Recall All is a selectable Dispatch option. `B` cancels a wheel. `A` and `B` retain select/confirm and back/cancel in
the passenger menu and never become altitude commands when closing it. Tool/descent
holds are cleared when entering a wheel and require a fresh press after leaving it.
Triggers remain available for altitude while using wheels. On foot, A remains jump.

Controller support is a remake-specific scheme; these changes route through the
existing flight, tool and dispatch implementations without changing retail simulation
rules. The original digital collective still uses a trigger threshold of 0.25;
opposed climb/descent commands cancel, and duplicate climb inputs do not add up.

The takeoff hint follows the last-used device. Keyboard/mouse activity shows every
live keyboard binding for climb and exit; controller activity shows the controller
alternatives (RT for takeoff, Y for exit). It updates while visible,
filters out the other device, and is hidden while a wheel or passenger menu is open.
Neutral stick noise, repeated held-axis reports and releases do not change device.

## Tool cards and contextual button help

The entire card containing the active tool has a broad, soft gold halo. The shared
water card glows for either the bucket or cannon, and the Apache card glows for either
weapon. Individual buttons do not receive selection borders.

The minimap carries a shared **TURBO** status module at its existing left-side light
and switch. Turbo ON lights the red lamp and flips the switch up; OFF dims it and
flips the switch down. The same display is used with keyboard and controller input.
A larger silver strip directly above the minimap carries yellow stencil lettering:
**F1: Controls** for keyboard/mouse, or **L3 + R3: Show help** for controllers. Opening
help changes the strip to the corresponding Hide controls / Hide help shortcut.
The controller chord works in either press order without changing Turbo.

`L3` or either `Shift` key toggles Turbo on release. Shift+F2–F5 recalls and minimap
chords do not toggle it. Airplanes retain their legacy L3 help shortcut as well.
Possession changes and input restoration clear Turbo. On foot, the Turbo indicator
and Turbo help rows are hidden; running controls continue to work. The on-foot
parachute status readout is removed, while pilot health and parachute controls remain.

The floating collapsed-help overlay and Photo Mode help row have been removed.
Photo Mode still opens with `Ctrl+P` or Esc → Photo Mode; `F12` captures an image
and Esc returns to play. Expanded contextual tool instructions remain available.

Expanded help uses imported Kenney Input Prompts icons. Flight help includes both
sticks, both triggers, and the X camera modifier alongside the current tool.
On-foot help explains movement, looking and aiming, both taser triggers, and actions.
Camera-adjust help shows zoom, lateral slide and trigger framing; remote-taser help
shows aiming, firing and returning to flight. Wheels and passengers retain their
contextual controls. Longer lists use two columns to fit above the map at 720p.
Exit helicopter appears whenever the existing exit rules permit it, including airborne exits.
The megaphone's broadcast row includes the current message; the D-pad icon explains
how to change it without broadcasting. A / Cross broadcasts. No top-of-screen tool
help panel is shown. Wheel and passenger control hints live in this list too.

L3 is no longer spotlight toggle. Hold X / Square and press A / Cross to toggle the spotlight.
D-pad spotlight aiming has been removed in both flight and camera-adjust mode.
Keyboard spotlight aiming remains unchanged.

The active input event's Unreal device descriptor selects Xbox or PlayStation glyphs,
including DualShock, DualSense and Sony identifiers. Switching controllers switches
icons. Unknown and XInput descriptors use Xbox glyphs; a remapping driver that exposes
a PlayStation pad only as a virtual Xbox pad hides the physical brand from Unreal.

Icons: [Kenney Input Prompts](https://kenney.nl/assets/input-prompts), CC0. The selected
PNG files and license are in Content/Slate/ControllerIcons and staged loose for Slate
in packaged builds. No engine plugin or external runtime dependency was added.
