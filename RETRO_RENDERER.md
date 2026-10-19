# Retro renderer: implementation and remaining differences

## Evidence from the supplied original

The existing read-only LE disassembly sets width 0x140 (320), height 0x190 (400) and viewport height 0x15e (350) in the mode initialization at disassembler addresses 0x320bd and 0x32124. The supplied DOSBox captures m_000.png through m_004.png show a 640x400 capture with doubled horizontal pixels and 25-line upper/lower margins around the 350-line footage. These support a logical 320x400 race grid with a 320x350 picture, rather than the port's previous 320x480 target. Original assets/captures are not redistributed.

The span loops around disassembler addresses 0x67e65 and 0x680ec advance packed texture coordinates with ADD/ADC, read a byte texture index, map it through a byte lookup table and write a byte framebuffer pixel. Other paths shift 16-bit fractional edge coordinates before writing spans. This supports affine texture stepping, integer raster positions and an indexed output. The GPU implementation reproduces the broad affine/pixel-grid behavior; it does not reproduce these assembly loops instruction for instruction.

## Implemented

- Each original HLZ/SPR/material/UBB is decoded with its own palette. The final
  framebuffer retains those source colors at six-bit VGA DAC precision before
  optional output-only filtering. White remains white. A video's changing
  palette never recolors the credits/UI or unrelated model textures. The 0.7.12
  whole-picture nearest-palette remap was removed because it altered correct
  colors when layers used different source palettes.
- Revived AI HUD/icons/cursor are bypassed in Retro, including when their cache
  was populated earlier. Original bitmap fonts and original sprites remain.
- Menus: 640x480. Races/replays: 320x400. The 640x480 logical UI viewport at y=30, height=420 maps to native race y=25, height=350. Output uses a 4:3 display shape and nearest-neighbor scaling.
- Texture filters, mipmap sampling, anisotropy, MSAA, FXAA and 60fps/intermediate poses are disabled in Retro. Cached video/canvas/model textures update their filter state when switching modes.
- Projected model vertices snap to the native viewport's integer grid. Retro texture varyings use screen-space affine interpolation, with quantized UVs and base-level nearest sampling.
- The Retro material path omits Revived's added specular, rim and two-light illumination. Original texture colors and the existing scene gain/pause treatment are retained.
- Original simulation update timing is selected. Camera interpolation is suppressed in Retro. This does not change physics/cheat preferences.
- The four Revived graphics preferences are saved once on entry, retained in the CFG and restored exactly on exit. Disabled rows are visibly gray, ignore mouse/keyboard activation and are skipped by keyboard navigation.

## Not yet equivalent to the DOS renderer

This remains an OpenGL rasterization path. Exact fixed-point transform overflow/rounding, near-plane clipping, scanline endpoint/fill rules, polygon ordering, source material/vertex shade-table interpretation and indexed transparency/shadow lookup behavior have not been reconstructed completely. RGB blending/depth testing still differ from the original byte-oriented renderer. The current layered RGB composition can contain more than 256 simultaneous colors even though each source is indexed and final channels retain six-bit DAC precision. An exact shared byte framebuffer, shade-table indices and layer palette reservations still require reconstruction; do not reintroduce a whole-frame nearest-video-palette approximation to claim a 256-color limit. Pixel-exact DOS shading for every model/material is not claimed.

## Color-depth research, 0.7.12

The supplied original executable provides the game-specific evidence: the
scanline paths at disassembler addresses 0x67e65 and 0x680ec fetch and write byte
indices; 0x32416..0x3242d streams RGB palette bytes through VGA DAC ports 0x3c8
and 0x3c9. Fade code around 0x32382 divides by 63 and iterates 63 intensity
steps. These findings agree with six-bit-per-primary VGA DAC operation.

The contemporary primary reference is the [VESA VBE 2.0 specification, revision
1.1](https://www.phatcode.net/res/221/files/vbe20.pdf), pages 14 and 30: a mode
set restores six-bit DAC precision; switching to eight-bit DAC precision needs
an explicit function 08h call. Eight bits per pixel means 256 palette indices,
not RGB332 and not eight bits for each color channel. Hardware support for VESA
640x480 does not prove that the game races at that resolution; the game's own
320x400 initialization and supplied DOSBox captures remain the race evidence.

This documentation distinguishes confirmed original behavior from the current
GPU approximation. Further matched-state DOSBox comparisons are still needed
for scan conversion, shade tables and index-based shadow/transparency rules.

The supplied DOSBox frames and current test runs use different race states/cameras, so they are not a pixel-exact A/B acceptance test. No invented scanlines, CRT blur, random jitter, arbitrary color-depth filter or deliberately injected clipping bugs are added as substitutes for verified original behavior. Recovering the remaining routines and comparing matched original frames is required before claiming full parity or "all quirks".

## Strict Retro, 0.7.14

The former effects Easter egg is removed. Legacy configuration cannot enable
modern effects. F9 operates only in Revived. On pause, the entire race viewport
moves down with the original 80-row menu, including video, models and HUD.
Original canvas pixels retain straight-alpha nearest sampling; premultiplied
filtering for Revived never alters Retro's CPU framebuffer.

See PHYSICS.md for the restored 0.7.13 driving/AI behavior, rolling-start
validation and remaining approximations. The rejected 0.7.14 integration is
research only. Full visual and simulation parity is not claimed.
