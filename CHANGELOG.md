0.7.15-dev (2026-10-19): rolled back the rejected 0.7.14 driving/contact integration to the accepted 0.7.13 behavior in both modes. Six-track/eight-car rolling-start traces match the stable reference in all 3,600 samples. Restored contact cooldowns; collisions vary among three original samples without consecutive repeats. The static Game Over screen is now silent at the mixer state boundary and no longer starts SCREAM.WAV. Strict Retro and independent graphics/audio teardown fixes remain.

0.7.14-dev (2026-10-19): original fixed-point driving and wall-response kernels, verified against the DOS executable; shared update cadence in both modes; removal of invented contact kicks and random heavy-barrel impulses. Removed the Retro effects Easter egg and legacy physics toggle. Premultiplied filtered canvas eliminates dark transparent-edge fringes in Revived. Retro pause displaces the entire scene. Engine-bus teardown blocks stale Game Over loops. Remaining parity limits are documented in PHYSICS.md and RETRO_RENDERER.md.

# Changelog

## 0.7.13-dev

0.7.13-dev (2026-10-19): global F1–F12 shortcuts, race-preserving mode-specific options, scrollable cinematic/model Extras, quality-98 JPEG screenshots and H.264/AAC MP4 recording with transient top-right notifications. Revived background filtering now includes original picture layers. Disabled arrows retain transparency. Retro keeps source palette colors instead of recoloring all layers through a video's changing palette. Shop notices use a small transparent overlay; the audio-off hint is beneath the sliders. Loading WAIT.16 music uses race volume even with menu music muted. The Retro-only surprise surprise! checkbox is the explicit opt-in effects-only exception; every other Retro lock is retained. See RECORDING.md.


## 0.7.12-dev

Revived-only AI remaster of HUD, free-standing equipment symbols, the cursor and the eleven explicitly approved bottom shop tiles. Other embedded buttons/backgrounds remain original. Strong backward grinding sparks and separate short wall/car impact bursts. Native Retro output is constrained to the active 256-color DAC palette before optional output filters.

Experimental GPU video upscaling with FSR1 EASU/RCAS, true HQ4x LUT filtering, xBR-lv2 at 4x and Lanczos3; bounded source-frame caching, saved selector, hidden/bypassed in Retro. Video export/import is recorded for later.

Includes 0.7.11: original directional lamp masks with minimum four-pixel red clusters for all fourteen cars; brighter front/rear lamps, brake intensification and trails; career/single preview HDR glow; stronger mine/barrel blasts; six effect intensity sliders and exact-default reset; consistent Revived naming.


## 0.7.9-dev

Revive Effects submenu: dynamic shadows, texture-shaped headlights/rear lights with brake glow and short trails, translucent oil, animated textured fire/smoke/3D debris, progressive damage/wrecks and contact sparks for all vehicles. Fix selected fractional output scaling, original metallic scrape audio, missile flight models/native size/height, and swept hits on explosive barrels/mines. See REVIVE_EFFECTS.md.


0.7.8 additions: English native Windows/Linux startup-mode chooser with optional saved choice; GOG-only original DOS launcher; final-frame Retro Point/Linear/HQ2x/BRZ/Interlacing/CRT filters, hidden in Revived.


## 0.7.8-dev - 2026-10-18

- Read all fourteen original car/engine assignments, four profile levels and the complete playback/gear curve directly from each verified original executable. Use TURBINE/M_2/MOT3 for the correct player/opponent car; preserve shared original profiles.
- Restore the 0.7.6 options layout and full-size original bitmap labels in Retro. Put independent volume sliders into Advanced sound, with coarse segmented pixel bars.
- Show only supported Retro graphics/debug settings. Suppress recovery, smoothed camera and Revived physics while preserving their saved Revived preferences; remove the Retro texture-dump action.
- Keep the compact scalable Revived options/menu path. Check all three languages for overlap at both menu sizes.

## 0.7.7-dev - 2026-10-18

- Rearrange options into two compact columns, remove dark panels and keep every label in its column in DE/EN/FR and Retro/Revived.
- Add a shared scalable SDF typeface for runtime game text throughout Revived; retain original bitmap fonts in Retro. Include Rajdhani SemiBold OFL notices and a reproducible atlas generator.
- Replace audio toggles with five independent 0-100 percent sliders, including player/opponent engines; support mouse dragging and keyboard/controller adjustment.
- Load saved percentages before audio, skip disabled categories, release stopped caches, gate video PCM decoding at zero and preserve silent video timing. Migrate legacy Boolean settings.

## 0.7.6-dev - 2026-10-18

- Substitute only explicit accent palette markers 248/249; preserve object white 250 and all other real colors. Check all 408 vehicle/object textures across the three editions and all six worlds.
- Make Dump textures a one-shot action. Repeated clicks retain the active job, leaving menus or changing viewer mode does not pause it, and old dump_textures config values no longer start exports.
- Reuse the track loading bar as a global export overlay, show completion briefly and play the Apply menu effect once at completion.
- Refresh unedited legacy dumps containing the previous palette mistake, preserving edited exports and all replacement PNGs.


## 0.7.5-dev - 2026-10-18

- Add an eager, incremental original-texture catalog exporter to texture_dumps, preserving source paths, palette variants, SPR alpha/placement and M3D material metadata.
- Add PNG replacements in texture_replacements for models, backgrounds, UI, sprites, HUD and font atlases. Keep original geometry, UV scale, hit boxes and glyph metrics; compose high-resolution UI images as ordered GPU layers.
- Restrict replacements to Revived, preserve the preference in Retro and add English diagnostic events and a complete custom_textures.txt guide.


## 0.7.4-dev - 2026-10-18

- Resolve all vehicle texture reserved colors (248..255) through PAL_RACE instead of source placeholder colors; preserve every ordinary palette color. Audit all 342 vehicle texture files across six worlds and three editions.
- Correct the horizontally mirrored shop/selection camera and increase turntable speed from 0.6 to 2.1 radians/second (3.5 times).
- Stop stale engine instances by sample identity, clear all mixer channels on Game Over and flush queued Windows/PulseAudio output. Preserve the finite Game Over entry effect.
- Capture all 14 cars in both graphics modes on Windows and Linux; test pixel palettes, turn speed, silent PCM after the entry effect and Windows queued-audio reset.

## 0.7.3-dev - 2026-10-18

- Lock texture filtering, MSAA, FXAA and framerate to the Retro settings, gray disabled controls and skip them during keyboard navigation. Persist a separate Revived snapshot and restore it on return, including after restart.
- Correct the Retro race/replay target to 320x400 with 350 native video lines, preserve 640x480 menus and use nearest sampling throughout. Restore anisotropic/mipmap filtering correctly on mode changes.
- Add integer projected-vertex snapping, affine texture interpolation and a Retro material path without the added Revived lighting. Record the distinction between this GPU approximation and unimplemented original scan-converter/shading-table quirks.

## 0.7.2-dev - 2026-10-18

- Verify each selected/local game directory once with an English native progress dialog. Persist Files_OK, verified directory and detected edition in the setup CFG; keep the receipt when saving settings. Cached starts check presence only, including the M.EXE loader. Explicit --validate still performs a full check.
- Rename the classic presentation to Retro Mode in all languages and use localized Scaling (Retro Mode) labels. Migrate the old scaling config keys.
- Log loaded graphics, models and videos, including track backgrounds, while excluding sound filenames. Remove periodic FPS output. Disable logging, history, file output and optional diagnostic work when the console is off.
- Add English transaction events for vehicles, shield upgrades, launchers, ammunition, repairs, payments and configuration changes.

## 0.7.1-dev - 2026-10-18

- Add a persisted Debug-menu console checkbox. Windows keeps one game-owned console with preserved history and red errors; Linux enables detailed terminal output. Log state/track/video/audio/settings events in English, independently of UI language. Protect against console Close and Quick Edit pauses.

- Review 271 text keys in German/English/French. Use modern Antialiasing/Framerate terminology, fix empty-slot and saved-game wording, translate missing replay/help/error strings and repair nested track-name fragments. Separate stable keys from German display values; render user names without translation. Check every catalog entry in every language and measure key labels with the actual bitmap font.

- Correct quit-menu Exit/Reset labels and French Quitter/Réinitialiser; use Debug / Cheats and Débogage / codes consistently.
- Offer only Original and 60fps, with automatic intermediate poses in 60fps only. Migrate old unlimited/2 settings to 60fps and discard the independent interpolation toggle.
- Fix saved game-directory lookup: the old case-folding operation changed underscores, preventing recognition of game_directory. Use a shared bounded parser for startup/settings, including quoted paths and UTF-8 BOMs.
- Prefer complete local game data, otherwise use the validated saved path; prompt only when both fail. Save the selected absolute path. Always show the missing-data introduction in English; cancellation exits normally.

## Earlier 0.7.0 work â€” 2026-10-18

Based on the supplied OpenMR2 0.6.4 ZIP. Local development candidate; not approved for public release.

- Restore track-specific original HUD weapon/mine/shield sprites and transparency, replacing the unrelated ARMES sheet. Use each track's original palette plus reserved HUD colors from PAL_RACE.
- Restore fine light bar frames, original reserved colors, relative positions and 320x400-to-640x480 scaling. Shield has up to 15 fill segments (65535 raw); a fresh hull has 16 (65536 raw). Keep displayed values bound to real shield/damage/ammo state.
- Stop and invalidate owned race audio loops on game-over/other race exits and before reinitializing the race. Preserve unrelated mixer channels.
- Normalize the language-dependent decoder-table offsets (EN -1696 bytes, FR -16, DE 0), including seven self-modifying instruction operands. Validate the entire 23,124-byte canonical block. Identical track frames/seeks are tested across languages.
- Add English configuration keys, symbolic race frame-rate values, legacy aliases, deterministic canonical-key precedence and migration backups. Import old saves/configs by copying.
- Add translation infrastructure, 271 English/French menu entries and French accent composition; translation coverage is still incomplete.
- Remove the ineffective input-device radio selector. Keyboard and Windows analog controller behavior is preserved. Retain working diagnostic modes and cheats.
- Add Windows-native build/test scripts, Linux tests, Linux PulseAudio/PipeWire client audio, X11 path selection and EWMH fullscreen, plus a real AppImage build and dependency inventory.
- Add reproducible local candidate packaging and release/test/license status documentation. No public upload or GitHub release was made.

## 0.7.10-dev

- Dedicated Revive missile effects: brief launch pulse, animated exhaust jet and bounded world-space smoke trails for Kard, Boomer and Hi-Trax, including opponent shots.
- Preserve smoke briefly after impacts; reset unrelated emitters on slot reuse and timeline changes.
- Use an arrow for the Revive Effects submenu link in Advanced Graphics. Add a localized independent Missile effects switch to the submenu.

## 0.7.11-dev

- Use original lamp markers, minimum four-pixel red clusters, body location,
  face direction and non-wheel nodes to prevent red paint/wheel emission.
- Brighter warm-white front lamps, stronger running rear-light trails and
  substantially brighter braking. Correct front lamps remain active on Blatt 96.
- Shared HDR lamp glow in career and single-race vehicle previews.
- Intense barrel/mine blast cores, shock rings, smoke and 48 brief solid debris pieces.
- Six 0-200% effect sliders, tuned 100% defaults, Reset and persistent settings.
- Visible branding consistently uses Revived, including Revived Effects and startup choice.
