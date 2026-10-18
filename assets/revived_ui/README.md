# Revived HUD and standalone symbols

These are built-in Imagegen edits of the supplied original MegaRace 2 sprites,
used exclusively in Revived Mode. Original assets are never modified. There are
49 selected sprites and 59 complete source/frame/palette identities: 26 HUD
variants, 11 standalone weapon/equipment icons, 11 explicitly approved bottom
shop-strip tiles and one cursor.

The generated source sheets are `hud-generated.png`, `arsenal-generated.png`
and `controls-generated.png` in this directory, plus the approved
`arsenal_buttons-generated.png` shop strip. Exact final prompts and tool
mode are recorded in `prompts.json` and `shop-strip-prompt.txt`. Generation used the built-in image_gen
tool in edit mode with transparent backgrounds, not a local interpolation
algorithm described as AI. The individual `sprite-NNN.png` assets are packed
from those sheets at eight times the original sprite dimensions. Alpha is kept
as straight RGBA with partial edge coverage. The eleven approved shop tiles
are normalized to 400x328 PNG canvases (50x41 logical pixels) after isolating
the main opaque tile component, ignoring stray transparent gutter speckles.
Revived uses equal 56-pixel horizontal steps and one common baseline; hit boxes
and the selection border use the same rectangles. Retro keeps the original
49/50-pixel widths, original positions and source graphics. The runtime draws these on ordered
GPU layers so their detail is not reduced to the original 640x480 canvas first.

`catalog.json` identifies every original candidate and whether it is included.
Unknown paths or palette hashes fall back to the original. An exact user PNG in
`revived_config/texture_replacements` has priority when custom textures are
enabled. Texture dumps continue to export the original game assets.

Background-integrated buttons are deliberately excluded: save/replay controls,
shop navigation, checkboxes and Apply/submenu buttons. The eleven framed weapon
icons are an explicit user-approved exception to the background-button rule.
Main-menu panels, portraits and backgrounds remain original. Standalone weapon
symbols retain their own small backing, without changing the surrounding shop.
AI redraws are artistic remaster assets; fine details and brightness can differ
from the low-resolution originals. Retro always uses the originals, including
the original cursor and palette variants.

To reproduce the packing, run `tools/prepare_ui_remaster.py` with the read-only
English fixture available, then `tools/build_ui_remaster.py`. The selected
generated sheets are already stored here; another AI generation is unnecessary.
The generated C++ registry is `src/remastered_ui_data.h`.
