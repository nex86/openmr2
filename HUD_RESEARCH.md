# HUD and decoder findings

Research used the supplied original installations and screenshots. Referenced addresses below are the relocated German GOG LE image, exported read only by `tools/inspect_executable.py`; no original executable/asset dump is included in packages.

## Icons: the actual error was resource selection

The old port decoded `DATA640/ARMES.HLZ` as a row-major 320x25 image, discovered groups of occupied columns, and assigned those groups to weapons/mine/oil/shield. The file really contains those elongated graphics; decoding it with alternate strides/planes or palettes does not recover the requested skull/shield. It is not the HUD resource selected by the original draw path.

The original path at 0x143b2/0x1440f selects the six frames loaded from the current track's `.SPR` file. The 0x13ca6 load uses the track stem plus `.SPR`. Frames are: 0 small missile, 1 medium missile, 2 large missile, 3 mine/skull, 4 oil, 5 shield. The Foundry frame dimensions are respectively 20x15, 17x12, 16x13, 14x23, 16x28, 20x22. Sprite records have a 24-byte header and tightly packed row-major pixels; index zero is transparent. These original pixels are used directly.

The associated track `.HLZ` supplies the palette. The original palette-loading helper replaces the filename extension with HLZ even where callers build a `.PAL` stem. A separate original load at 0x13e09 merges the five reserved colors 251..255 from `DATA640/PAL_RACE.HLZ`. In particular, 251 is black, 252 white, 254 red. Using only the track palette leaves status-bar colors black. No hand-drawn icon replacement, image-generation substitution or arbitrary red recoloring was used.

## Layout, bars and values

The logical race mode is 320x400. The port's 640x480 canvas therefore uses x2 and y1.2 scaling for the HUD rather than uniform x2 scaling. Original positions from the draw path map to approximately:

| Element | Logical position | Canvas position including HUD origin y=30 |
|---|---|---|
| Front weapon | 7,4 | 14,35 |
| Rear weapon | 48,0 | 96,30 |
| Shield | 89,0 | 178,30 |
| Shield bar | 80,26 | 160,61 |
| Hull bar | 200,10 | 400,42 |

Original routines 0x14539 and 0x145d4 advance fill left-to-right with two logical pixels per segment, alternating a colored stroke with a black stroke. The first five colored strokes use palette 254; later strokes use 252. Frame dimensions are 34x7 for hull and 34x5 for shield, with inclusive edges in the original rectangle routine. The original thick red replacement frame is removed.

The bar input is shifted by 11 then divided by two. Shield is capped at 0xFFFF, so a full shield has 15 segments. Vehicle initialization uses 0x10000 for fresh hull state (+0x80 in the original vehicle records); transfer reads it into the state displayed at 0xe1c40. That full hull therefore has 16 segments. Boundary tests distinguish these cases. The port converts actual current shield percentage and remaining hull percentage (`1 - damage/100`) to these separate scales. Ammo, rank and laps continue to come from the live race state. No reference screenshot's numbers/fill level is copied.

Canvas letterboxing/scaling preserves these proportions on wider windows. Full typography parity is not complete: the inherited FONTE21 HUD font/metrics and some text placement still differ from the reference. Track/scenery, palette shades and current race state differ between the supplied crop and test runs. The comparisons use the supplied original at a common 640-pixel HUD width, matched using the known sprite widths; the differently scaled upper old-port capture is not used to infer raw dimensions.

## English/French decoder corruption

The statically translated decoder references German data addresses. English strings shift the decoder-table block by -1696 bytes; French shifts it by -16 bytes. The old sanity check only required a pointer to fall within the decoder code range, so incorrect shifted entries could pass and still produce corrupt images.

All 23,124 relocated table bytes from 0x98220 to 0x9dc74 are identical across the three supplied versions after accounting for their offsets. The shared SHA-256 is `3ee0a5ed437793d45603b6f90d415683f1f825d5c8207f540da9a4f5d3b766e0`. Normalization copies this block in memory, backwards because ranges overlap, and adjusts the seven initial self-modifying operands at 0x25546, 0x26751, 0x285aa, 0x285ba, 0x29972, 0x29982, 0x2a9d6. Original installation files remain unchanged. The complete normalized block is verified before use, not just one pointer.

Actual scenery frames 0 and 150 and a backward seek to 30 now decode to identical RGBA hashes across all three languages. Initial broken renders were observed before the fix and are not counted as passes.
