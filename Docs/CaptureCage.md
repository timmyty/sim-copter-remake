# Capture cage controls and skid deployment

User-requested remake addition, October 10, 2026. The earlier cage purchase had keyboard/controller support but no tool flap; its rendering also returned immediately when stowed.

The equipment bit now reveals a panel immediately below the megaphone. It reuses the original flap frame/grid, octagonal buttons, label/readout colors, selected-tool glow and HUD scaling. LOWER and RAISE are held actions; OPEN releases the floor. Controls never take keyboard focus, mouse holds survive the controller-input refresh, and switching tools or resetting input cancels the hold. The cage panel also participates in mission-marker avoidance.

Each fitted helicopter keeps compact steel/yellow emitter rails on its skids. The mount follows the actual airframe's bank, lowest point and width. Deployment extends bars from that mount toward the lowered rim; a two-sided grille closes automatically after capturing people. Reeling waits for the seal to finish. Empty searches leave the floor open. Raising to the top boards real occupants through the existing capacity/custody rules, then stows an empty cage. An open/release cooldown prevents immediate recapture. Tow clamps retain their cable and clamp graphics.

The saved payload is unchanged. Formation and seal presentation are reconstructed from the existing deployed/open flags. Existing purchased cages gain the panel and fittings when their save loads.

The crime-family scheduler uses `3 * max(base crime weight, sum(base weights) / 7)` while any player-owned aircraft has a purchased cage. The sum is renormalized; the base city tuning remains unchanged. This adds crime to early cities while preserving all-zero/free-roam settings, existing difficulty-dependent crime types, concurrency limits and plane-only scheduling. The AI helper and temporary debug grants do not count as purchases. Selling the final cage restores the original distribution.

Validation evidence is recorded under `Docs/scratchpad/capture-cage/` and in `Docs/validation/CaptureCageResults.json`. No foreground gameplay session is required by this update; automated logic checks and offscreen renders cover the feature, with interactive control feel left to the player.
