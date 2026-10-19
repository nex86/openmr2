# Configuration and migration

Format: `key = value`, one setting per line; `#` starts a comment. UI language never changes the format. All current keys/comments and symbolic values written by the port are English. User paths and saved names are copied without translation.

| Key | Values / purpose |
|---|---|
| `game_directory` | Original-data path |
| `Files_OK` | `true` after successful one-time verification; `false` after failure or to request rechecking |
| `verified_directory`, `verified_language` | Path bound to Files_OK and detected edition (0 English, 1 French, 2 German); preserved when saving settings |
| `texture_filter` | 0 filtered/high detail, 1 nearest/low detail; inherited numeric meaning |
| `graphics_mode` | 0 Revived, 1 Retro Mode |
| `retro_scale` | 1â€“4 |
| `integer_scale` | 0/1; constrain Retro Mode scaling to whole factors |
| `msaa` | 0, 2, 4, 8 (menu); parser also retains up to 16 from existing settings, subject to driver support |
| `fxaa`, `fullscreen` | 0/1 |
| `frame_rate` | `original`, `60fps` |
| `sound_effects`, `race_music`, `menu_music`, `menu_sounds` | 0/1 |
| `lance_boyle_videos` | 0/1 |
| `laps` | 0 original 5/5/3 heats, or override 3/5 |
| `pad_accelerate`, `pad_brake`, `pad_left`, `pad_right` | Controller binding index 0â€“25 |
| `pad_fire_front`, `pad_fire_rear`, `pad_switch_front`, `pad_switch_rear`, `pad_pause` | Controller binding index 0â€“25 |
| `debug_console` | 0/1; Windows debug console, Linux detailed terminal diagnostics; diagnostics always English |
| `debug_fps`, `debug_race_info`, `debug_road_markers`, `debug_no_opponents` | 0/1 |
| `debug_stretch`, `debug_legacy_camera`, `debug_skip_videos` | 0/1; experimental/diagnostic presentation options |
| `simulation_model` | 0 one complete step per reconstructed original update, 1 older subdivided Revived model |
| `ai_rescue` | 0/1 modern recovery assistance |
| `cheat_money`, `cheat_ammo`, `cheat_weapons_and_cars`, `cheat_no_damage` | 0/1; championship cheats |

Binding indices: 0â€“3 D-pad up/down/left/right; 4 Start; 5 Back/View; 6/7 stick click; 8/9 LB/RB; 10/11 reserved; 12â€“15 A/B/X/Y; 16/17 LT/RT; 18â€“21 left stick left/right/up/down; 22â€“25 right stick left/right/up/down. Remapping UI does not offer the reserved indices.

Single-race settings/cheats remain in the existing binary `openmr2_single.dat`; save/replay formats are unchanged. These binary records contain no language-dependent section names, textual keys, symbolic selections or explanatory comments.

## Legacy aliases

The complete authoritative rename table is `src/config_keys.h`. Examples:

| Legacy | Current |
|---|---|
| `spielverzeichnis` | `game_directory` |
| `grafik_detail`, `grafikmodus` | `texture_filter`, `graphics_mode` |
| `oldskool_skalierung`, `ganzzahlige_skalierung` | `retro_scale`, `integer_scale` |
| `vollbild` | `fullscreen` |
| `bildausgabe_rennen`, `debug_bildausgabe` | `frame_rate` |
| `soundeffekte`, `musik_rennen`, `musik_hauptmenue`, `geraeusche_menues` | `sound_effects`, `race_music`, `menu_music`, `menu_sounds` |
| `runden`, `fahrmodell`, `ki_notfallhilfe` | `laps`, `simulation_model`, `ai_rescue` |
| `pad_gas`, `pad_bremse`, `pad_links`, `pad_rechts` | `pad_accelerate`, `pad_brake`, `pad_left`, `pad_right` |
| `pad_frontwaffe`, `pad_heckwaffe`, `pad_frontwaffe_wechseln`, `pad_heckwaffe_wechseln` | `pad_fire_front`, `pad_fire_rear`, `pad_switch_front`, `pad_switch_rear` |
| German `debug_*` and `cheat_*` keys | Corresponding English diagnostic/cheat names in the table |

English canonical settings take precedence regardless of file order; the older `debug_bildausgabe` alias has lower precedence than `bildausgabe_rennen`. Legacy `unlimited` and numeric value 2 migrate to `60fps`; values 0/1 retain Original/60fps. Old interpolation keys are ignored and removed when settings are saved. Within the same precedence, the last occurrence wins. Numeric values are constrained to supported ranges; lap overrides other than 3/5 become original.

The first migrated text file is copied to `openmr2_setup.cfg.pre-0.7`. Old binary setup files are read and retained. Original installation settings/saves are imported by copying, never deleting the source.

Removed: `steuerung`/`controls` and its Controller/Mouse/Keyboard radio selector. The value was stored and shown but never used by race input, so there is no behavior to preserve. Keyboard and Windows XInput handling remain enabled. The separate interpolation option is removed; it now follows the selected frame-rate mode automatically.

## One-time verification and diagnostics

The initial local-data lookup and selected/saved folders use the same verification cache. The native English progress dialog shows the current file and byte progress. A true result is reused only for its recorded path and supported edition. Future starts perform presence checks, with no CRC/SHA processing of the game files or decoder tables. Required-file removal invalidates the result. Content changes to existing files require setting `Files_OK = false`; `--validate` always performs a full check on explicit request.

Retro Mode keeps the same name in every UI language. `retro_scale` replaces the former scaling key; legacy aliases remain readable. `--retro-scale N` selects Retro Mode for developer captures.

When `debug_console = 0`, console messages are discarded before history, debugger output or disk logging. File classification, event formatting and optional video/camera diagnostics are disabled. The game does not collect a background console history. Explicit developer `--trace` capture also requires the console to be enabled. When enabled, log loaded graphics/models/videos (including track background UBB files), state transitions, settings changes, purchases, sales and repairs in English. Audio filenames are excluded. No periodic FPS status is emitted.

## Revived preferences retained by Retro Mode

`revived_options_saved = 1` marks a saved snapshot. `revived_texture_filter`, `revived_msaa`, `revived_fxaa` and `revived_frame_rate` retain the previous Revived settings. The snapshot frame rate uses 0 Original / 1 60fps. While Retro Mode is active, the ordinary active settings read `texture_filter = 1`, `msaa = 0`, `fxaa = 0`, `frame_rate = original`. Re-selecting Retro does not overwrite the snapshot. Returning to Revived restores the previous values exactly. Upgrading an older Retro configuration captures its preferences once before enforcing the active restrictions.


## Custom textures (0.7.5)

Debug / Cheats > Dump textures exports original static assets and material references to texture_dumps. Advanced Graphics > Enable custom textures loads edited PNGs only from texture_replacements, keeping source-relative LOCAL/CD paths and palette-qualified SPR frame names. Replacements are suppressed in Retro while the Revived preference is retained. Higher-resolution UI is composed as ordered GPU layers; geometry, hit boxes and original UV normalization remain unchanged. English diagnostics describe exports, loads, alpha, fallback and cache reload. Read custom_textures.txt for complete instructions and limits. UBB video frames and procedural graphics are outside the static-image replacement feature.


## 0.7.6 palette and export action corrections

Only explicit accent marker colors at palette indices 248/249 are replaced; all other entries, including white barrel skulls, are preserved. Dump textures is an action, with no saved toggle or cancellation on repeated clicks. Export progresses in every view, displays the same bar as track loading, stops automatically and plays the Apply sound once. Existing dump_textures config keys are ignored. Corrected original dumps refresh only exact, unchanged legacy outputs; edited exports/replacement PNGs are preserved.

Texture export/replacement folders are stored inside revived_config: revived_config/texture_dumps and revived_config/texture_replacements.


## 0.7.7 audio percentages and scalable text

`sounds_volume`, `engines_volume`, `race_music_volume`, `menu_music_volume` and
`menu_sounds_volume` accept integers from 0 to 100. They are independent and
apply immediately. Percentages scale the existing category mix (100 preserves
its original gain). The options screen supports mouse clicking/dragging and
keyboard/controller Up/Down for focus, Left/Right for 5-percent steps.

0 prevents the category's samples/music from loading or starting playback.
Turning a category down to 0 stops its active voices and frees its cached audio.
Re-enabling it loads audio on demand. Loading-screen music follows Menu music.
Sounds covers race effects and speech/video sound; Engines covers the player
and opponent engine loops. Video pictures still play when Sounds is 0; embedded
audio is not decoded and video timing is preserved. Menu effects, including the
texture-export completion effect, follow Menu sounds.

Legacy sound_effects/race_music/menu_music/menu_sounds Boolean values are still
read; when a percentage key is absent, off migrates to 0 and on to 100. The
initial engine percentage follows legacy sound_effects. Explicit percentages
take precedence. Compatible Boolean mirrors are saved for older versions.

All runtime game text uses an embedded scalable Rajdhani SemiBold distance-field
atlas in Revived, including menus, shops, messages, results, credits and race HUD.
Retro uses the original bitmap fonts and original text rendering. French and
German characters are present in the scalable atlas. Original text baked into
artwork or video remains part of that artwork/video. A custom original-font
atlas, if supplied and enabled, overrides the built-in Revived typeface for that
font and retains the original atlas's glyph geometry.


## 0.7.8 Retro menus and original car audio

Retro restores the 0.7.6 options layout with its full-size original fonts.
Audio percentages are configured in Advanced sound (German: Erweiterter Sound;
French: Audio avancé). Its coarse segmented bars support the same mouse,
keyboard and mapped-controller controls and share the same saved percentages.
Revived retains its compact main options screen and scalable text.

Retro advanced graphics shows only integer scaling, scaling factor and
fullscreen. Retro Debug omits stretched-picture, smoothed-camera, AI recovery,
Revived physics and Dump textures. Effective recovery, smoothed camera and
Revived physics are disabled in Retro; their stored values are retained for
Revived and are not reset by the Retro Debug Reset action.

Engine audio uses the original per-car profile and speed/frequency table from
the verified original executable in both modes, for the player and the audible
nearby opponent. Several cars share original profiles; see ENGINE_AUDIO.md.
The Engines slider and its 0% loading/playback gates apply to all profiles.


## Startup mode and original DOS launcher (0.7.8)

Before creating the game window, Windows shows a native English dialog with
**Revived Mode**, **Retro Mode**, and **Remember my choice**. Linux shows the
same choices in a native X11 window (also usable through Xwayland). Checking
Remember my choice saves `startup_mode` (0 Revived, 1 Retro) and
`remember_startup_mode = 1` in `revived_config/openmr2_setup.cfg`. Subsequent
launches use that selection without asking. With the box unchecked, the chosen
mode still starts immediately, but the dialog returns next launch. Closing the
dialog cancels startup. Later in-game mode changes do not alter the remembered
startup choice. Set `remember_startup_mode = 0` to display the chooser again.
Automated screenshot/export/demo/validation entry points skip the chooser.

For recognized GOG editions, the black-flag menu also offers **Run DOS Version**
(**DOS Version starten**, **Lancer la version DOS**). Unknown editions never
show this entry. Windows starts `DOSBOX/DOSBox.exe` from the selected game's
`DOSBOX` directory with the original shortcut arguments:

```text
-conf "..\dosboxmegarace2.conf" -conf "..\dosboxmegarace2_single.conf" -noconsole -c "exit"
```

OpenMR2 closes only after process creation succeeds. Missing executable/config
files or launch failure leave OpenMR2 running with an error message. Linux uses
an installed native `dosbox` executable with the same GOG configurations and
working directory; the Windows executable is not run through a shell or Wine.
Successful process creation cannot guarantee that DOSBox subsequently starts
the game successfully; its own configuration/errors still apply.

## Retro output filters (0.7.8)

Retro Advanced Graphics offers **Point**, **Linear**, **HQ2x**, **BRZ**,
**Interlacing**, and **CRT Filter**. `retro_output_filter` stores respectively
`point`, `linear`, `hq2x`, `brz`, `interlacing`, or `crt`. Point is the default.
All filters apply only to the final native framebuffer. They do not alter
textures, game resolution, timing, physics or menu hit boxes. HQ2x uses the
original HQ2x lookup-table shader; BRZ uses the xBRZ freescale shader. Interlacing
alternates dark native rows; CRT adds scanlines, RGB phosphor modulation and a
subtle vignette. These last two are presentation effects, not emulation of a
specific monitor or interlaced video hardware. Retro filter and scaling controls
are hidden in Revived; their saved values are restored on return to Retro.
The switch back to Retro remains available in Revived.

Filter sources, lookup table, full upstream licenses and SHA-256 provenance
are in `src/third_party/output_filters`; see `THIRD_PARTY_NOTICES.txt`.

## Revived Effects (0.7.9)

`real_time_shadows`, `vehicle_lights`, `enhanced_oil`, `modern_explosions` and `damage_effects` use 0/1 and default to 1. The submenu is hidden in Retro; saved preferences are retained. Damage: white smoke/light deformation at 70%, visibly stronger deformation at 80%, black smoke at 85%, fire at 90% (growing each percentage point), destroyed wreck/large explosion at 100%. See REVIVE_EFFECTS.md for details.

Scaling now honors the selected maximum factor even with `integer_scale = 0`; only fitting to smaller windows can reduce it.

## Missile exhaust (0.7.10)

`missile_effects = 1` enables launch pulses, small animated jet flames and fading world-space smoke trails for all three missile types in Revived Mode. Set to 0 to disable and clear them. The preference is independent of modern explosions, persists across mode changes and has no effect on Retro rendering.

## Revived effect intensity (0.7.11)

`shadow_intensity`, `light_intensity`, `oil_intensity`, `explosion_intensity`,
`damage_intensity`, `missile_intensity`: each 0-200, default 100. Zero suppresses
that effect while retaining its checkbox preference. Reset restores enabled
switches and 100% values. Retro preserves these settings without rendering the
effects. See REVIVE_EFFECTS.md for mapping and lamp restrictions.
# Experimental GPU video upscaling (0.7.12)

`video_upscaling = 1` enables runtime filtering of cinematic and track videos in
Revived. `video_filter = 0/1/2/3` selects FSR, HQ4x, 4xBR or Lanczos3. Default:
disabled, with FSR selected. Retro suppresses the feature and retains both
preferences. See VIDEO_UPSCALING.md for processing, caching and limits.


## 0.7.13 controls and capture

`effects_muted = 0/1` temporarily mutes all six modern effect groups without
changing their individual saved switches/intensities. The obsolete
`retro_surprise` key is ignored and removed on save. Retro always disables all
modern effect categories. The legacy `simulation_model` setting is also ignored;
both modes use the original driving kernel without physics substeps.

F1-F9 preference changes are saved in the same config. F10 does not reset a race.
F11 stores H.264/AAC MP4s in `revived_config/recordings`; F12 stores quality-98 JPGs
in `revived_config/screenshots`. See RECORDING.md for encoder setup and limits.
Loading WAIT.16 music uses `race_music_volume`, independently of menu volume.
The message beneath the audio sliders is localized as "0% = off" (German:
"0% = ausgeschaltet"). Capture notifications are localized and top-right.
