# Controller menus and city website links — October 15, 2026

R3 now opens a persistent Tools wheel. The second click switches to Dispatch;
further clicks alternate. Releasing R3 only clears the physical hold used by
map/help/radio chords. It never confirms a choice or closes a wheel. A confirms
the last highlighted option, B cancels, and X remains an alternate page switch.
On-screen help, passenger/remote-taser instructions and controller tests reflect
the new controls. The original release-to-close behavior reproduced in both
targeted baseline tests; all 17 focused controller/gameplay checks then passed.

The Open A SimCity File picker has underlined links to
`https://cities.opencity2k.org/` and `https://patcoston.com/sc2k/cities.asp`.
Each first opens an in-game confirmation displaying the full destination and:
“You're about to leave the game for a third-party website in your browser.
We don't control this site or its content.” Cancel is selected initially.
Only Open site launches the browser. Escape/B cancel the warning, both outcomes
restore the same city selection, and repeated input cannot launch twice.

All 15 focused frontend/navigation tests passed, including the new website test
with a captured browser callback rather than an actual browser launch. A D3D12
offscreen rerun also passed; the picker and warning were inspected with original
art and the missing-art fallback. No physical-controller playtest or external
browser launch was performed.

Evidence and scoped source backups: `Docs/scratchpad/radial-click-2026-10-15/`.
Publication keeps v1.0.16 at the user's request; final build, installation and
release verification are recorded in the validation receipt for this change.

Fresh publication Editor, Shipping and Development builds passed; all 28 focused
publication checks and both hidden 30-second packaged startups passed.
[Validation receipt](../validation/ControllerAndCityLinksResults.json).
