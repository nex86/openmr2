# OpenMR2 Documentation

Settings, controls, technical notes, development history and licensing.

## Contents

- [Configuration and migration](#configuration)
- [Keyboard and mouse controls](#keyboard)
- [Screenshots and recordings](#recording)
- [Revived Effects - 0.7.11](#revive-effects)
- [GPU video upscaling (Revived only)](#video-upscaling)
- [Driving and AI - 0.7.17-dev](#physics)
- [Retro renderer: implementation and remaining differences](#retro-renderer)
- [Building from source](#building)
- [Contributing](#contributing)
- [Source and release layout](#distribution)
- [License scope](#licensing)
- [License and redistribution status - provenance updated 2026-10-19](#license-status)
- [GitHub publication audit](#publication-audit)
- [Changelog](#changelog)
- [Release notes](#release-notes)
- [Deferred work](#todo)
- [Original engine audio](#engine-audio)
- [HUD and decoder findings](#hud-research)
- [Localization review - 0.7.6-dev](#localization-review)
- [Vehicle fire asset](#assets-effects-readme)
- [Revived HUD and standalone symbols](#assets-revived-ui-readme)
- [Publication build validation](#docs-validation)
- [Vendored PNG libraries](#src-third-party-readme)

---

<a id="configuration"></a>

## Configuration and migration

<a id="configuration--configuration-and-migration"></a>


Format: `key = value`, one setting per line; `#` starts a comment. UI language never changes the format. All current keys/comments and symbolic values written by the port are English. User paths and saved names are copied without translation.

| Key | Values / purpose |
|---|---|
| `game_directory` | Original-data path |
| `Files_OK` | `true` after successful one-time verification; `false` after failure or to request rechecking |
| `verified_directory`, `verified_language` | Path bound to Files_OK and detected edition (0 English, 1 French, 2 German); preserved when saving settings |
| `texture_filter` | 0 filtered/high detail, 1 nearest/low detail; inherited numeric meaning |
| `graphics_mode` | 0 Revived, 1 Retro Mode |
| `retro_scale` | 1–4 |
| `integer_scale` | 0/1; constrain Retro Mode scaling to whole factors |
| `msaa` | 0, 2, 4, 8 (menu); parser also retains up to 16 from existing settings, subject to driver support |
| `fxaa`, `fullscreen` | 0/1 |
| `frame_rate` | `original`, `60fps` |
| `sound_effects`, `race_music`, `menu_music`, `menu_sounds` | 0/1 |
| `lance_boyle_videos` | 0/1 |
| `laps` | 0 original 5/5/3 heats, or override 3/5 |
| `pad_accelerate`, `pad_brake`, `pad_left`, `pad_right` | Controller binding index 0–25 |
| `pad_fire_front`, `pad_fire_rear`, `pad_switch_front`, `pad_switch_rear`, `pad_pause` | Controller binding index 0–25 |
| `debug_console` | 0/1; Windows debug console, Linux detailed terminal diagnostics; diagnostics always English |
| `debug_fps`, `debug_race_info`, `debug_road_markers`, `debug_no_opponents` | 0/1 |
| `debug_stretch`, `debug_legacy_camera`, `debug_skip_videos` | 0/1; experimental/diagnostic presentation options |
| `simulation_model` | 0 one complete step per reconstructed original update, 1 older subdivided Revived model |
| `ai_rescue` | 0/1 modern recovery assistance |
| `cheat_money`, `cheat_ammo`, `cheat_weapons_and_cars`, `cheat_no_damage` | 0/1; championship cheats |

Binding indices: 0–3 D-pad up/down/left/right; 4 Start; 5 Back/View; 6/7 stick click; 8/9 LB/RB; 10/11 reserved; 12–15 A/B/X/Y; 16/17 LT/RT; 18–21 left stick left/right/up/down; 22–25 right stick left/right/up/down. Remapping UI does not offer the reserved indices.

Single-race settings/cheats remain in the existing binary `openmr2_single.dat`; save/replay formats are unchanged. These binary records contain no language-dependent section names, textual keys, symbolic selections or explanatory comments.

<a id="configuration--legacy-aliases"></a>

### Legacy aliases

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

<a id="configuration--one-time-verification-and-diagnostics"></a>

### One-time verification and diagnostics

The initial local-data lookup and selected/saved folders use the same verification cache. The native English progress dialog shows the current file and byte progress. A true result is reused only for its recorded path and supported edition. Future starts perform presence checks, with no CRC/SHA processing of the game files or decoder tables. Required-file removal invalidates the result. Content changes to existing files require setting `Files_OK = false`; `--validate` always performs a full check on explicit request.

Retro Mode keeps the same name in every UI language. `retro_scale` replaces the former scaling key; legacy aliases remain readable. `--retro-scale N` selects Retro Mode for developer captures.

When `debug_console = 0`, console messages are discarded before history, debugger output or disk logging. File classification, event formatting and optional video/camera diagnostics are disabled. The game does not collect a background console history. Explicit developer `--trace` capture also requires the console to be enabled. When enabled, log loaded graphics/models/videos (including track background UBB files), state transitions, settings changes, purchases, sales and repairs in English. Audio filenames are excluded. No periodic FPS status is emitted.

<a id="configuration--revived-preferences-retained-by-retro-mode"></a>

### Revived preferences retained by Retro Mode

`revived_options_saved = 1` marks a saved snapshot. `revived_texture_filter`, `revived_msaa`, `revived_fxaa` and `revived_frame_rate` retain the previous Revived settings. The snapshot frame rate uses 0 Original / 1 60fps. While Retro Mode is active, the ordinary active settings read `texture_filter = 1`, `msaa = 0`, `fxaa = 0`, `frame_rate = original`. Re-selecting Retro does not overwrite the snapshot. Returning to Revived restores the previous values exactly. Upgrading an older Retro configuration captures its preferences once before enforcing the active restrictions.


<a id="configuration--custom-textures-075"></a>

### Custom textures (0.7.5)

Debug / Cheats > Dump textures exports original static assets and material references to texture_dumps. Advanced Graphics > Enable custom textures loads edited PNGs only from texture_replacements, keeping source-relative LOCAL/CD paths and palette-qualified SPR frame names. Replacements are suppressed in Retro while the Revived preference is retained. Higher-resolution UI is composed as ordered GPU layers; geometry, hit boxes and original UV normalization remain unchanged. English diagnostics describe exports, loads, alpha, fallback and cache reload. Read custom_textures.txt for complete instructions and limits. UBB video frames and procedural graphics are outside the static-image replacement feature.


<a id="configuration--076-palette-and-export-action-corrections"></a>

### 0.7.6 palette and export action corrections

Only explicit accent marker colors at palette indices 248/249 are replaced; all other entries, including white barrel skulls, are preserved. Dump textures is an action, with no saved toggle or cancellation on repeated clicks. Export progresses in every view, displays the same bar as track loading, stops automatically and plays the Apply sound once. Existing dump_textures config keys are ignored. Corrected original dumps refresh only exact, unchanged legacy outputs; edited exports/replacement PNGs are preserved.

Texture export/replacement folders are stored inside revived_config: revived_config/texture_dumps and revived_config/texture_replacements.


<a id="configuration--077-audio-percentages-and-scalable-text"></a>

### 0.7.7 audio percentages and scalable text

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


<a id="configuration--078-retro-menus-and-original-car-audio"></a>

### 0.7.8 Retro menus and original car audio

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


<a id="configuration--startup-mode-and-original-dos-launcher-078"></a>

### Startup mode and original DOS launcher (0.7.8)

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

On Windows with recognized GOG editions, the black-flag menu also offers **Run DOS Version**
(**DOS Version starten**, **Lancer la version DOS**). Unknown editions never
show this entry. Windows starts `DOSBOX/DOSBox.exe` from the selected game's
`DOSBOX` directory with the original shortcut arguments:

```text
-conf "..\dosboxmegarace2.conf" -conf "..\dosboxmegarace2_single.conf" -noconsole -c "exit"
```

OpenMR2 closes only after process creation succeeds. Missing executable/config
files or launch failure leave OpenMR2 running with an error message. Linux does
not show this menu entry or launch DOSBox.
Successful process creation cannot guarantee that DOSBox subsequently starts
the game successfully; its own configuration/errors still apply.

<a id="configuration--retro-output-filters-078"></a>

### Retro output filters (0.7.8)

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

<a id="configuration--revived-effects-079"></a>

### Revived Effects (0.7.9)

`real_time_shadows`, `vehicle_lights`, `enhanced_oil`, `modern_explosions` and `damage_effects` use 0/1 and default to 1. The submenu is hidden in Retro; saved preferences are retained. Damage: white smoke/light deformation at 70%, visibly stronger deformation at 80%, black smoke at 85%, fire at 90% (growing each percentage point), destroyed wreck/large explosion at 100%. See REVIVE_EFFECTS.md for details.

Scaling now honors the selected maximum factor even with `integer_scale = 0`; only fitting to smaller windows can reduce it.

<a id="configuration--missile-exhaust-0710"></a>

### Missile exhaust (0.7.10)

`missile_effects = 1` enables launch pulses, small animated jet flames and fading world-space smoke trails for all three missile types in Revived Mode. Set to 0 to disable and clear them. The preference is independent of modern explosions, persists across mode changes and has no effect on Retro rendering.

<a id="configuration--revived-effect-intensity-0711"></a>

### Revived effect intensity (0.7.11)

`shadow_intensity`, `light_intensity`, `oil_intensity`, `explosion_intensity`,
`damage_intensity`, `missile_intensity`: each 0-200, default 100. Zero suppresses
that effect while retaining its checkbox preference. Reset restores enabled
switches and 100% values. Retro preserves these settings without rendering the
effects. See REVIVE_EFFECTS.md for mapping and lamp restrictions.

<a id="configuration--experimental-gpu-video-upscaling"></a>

## Experimental GPU video upscaling

`video_upscaling = 1` enables runtime filtering of cinematic and track videos in
Revived. `video_filter` accepts the following values:

| Value | Filter |
| --- | --- |
| 0 | FSR |
| 1 | HQ4x |
| 2 | 4xBR |
| 3 | Lanczos3 |
| 4 | FSRCNNX AI (8 channels) |
| 5 | FSRCNNX AI HQ (16 channels) |

Fresh configurations default to disabled, with FSR selected. The existing
values 0-3 keep their previous meaning. The AI choices run embedded learned
2x models on the OpenGL GPU; HQ uses a larger network and costs more GPU time.
Results are cached per decoded frame. Disabling upscaling or entering Retro
bypasses inference and filtering while retaining the Revived preferences.
FSRCNNX failures fall back to Lanczos3, then original sampling if necessary.
See VIDEO_UPSCALING.md for processing, image limits and remaining artifacts.


<a id="configuration--0713-controls-and-capture"></a>

### 0.7.13 controls and capture

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


---

<a id="keyboard"></a>

## Keyboard and mouse controls

<a id="keyboard--keyboard-and-mouse-controls"></a>


<a id="keyboard--global-shortcuts"></a>

### Global shortcuts

| Key | Action |
| --- | --- |
| F1 | Switch between Revived and Retro Mode |
| F2 | Toggle texture/background filtering in Revived |
| F3 | Switch Original / 60 FPS in Revived |
| F4 | Toggle custom textures in Revived |
| F5 | Cycle Retro output filters: Point, Linear, HQ2x, BRZ, Interlacing, CRT |
| F6 | Cycle Retro scaling: 1x, 2x, 3x, 4x |
| F7 | Toggle experimental video upscaling in Revived |
| F8 | Cycle experimental video filters: FSR, HQ4x, 4xBR, Lanczos3, FSRCNNX AI, FSRCNNX AI HQ |
| F9 | Mute/restore all six Revived effect groups, preserving their preferences |
| F10 | Open the current mode's full options; Apply/Esc returns to the paused scene |
| F11 | Start/stop H.264/AAC MP4 recording; external FFmpeg required |
| F12 | Save a quality-98 JPEG screenshot |
| Alt+Enter | Toggle fullscreen on Windows; Linux uses the fullscreen menu option |

Modern graphics shortcuts, including F9, remain unavailable in Retro Mode.
Capture notifications appear briefly at the top right and are excluded from
saved media. Files go to `revived_config/recordings` and
`revived_config/screenshots`.

<a id="keyboard--driving-and-racing"></a>

### Driving and racing

| Key | Action |
| --- | --- |
| Up / Down | Accelerate / brake; reverse follows the original drive behavior |
| Left / Right | Steer |
| Space | Fire selected front weapon |
| Enter | Use selected rear weapon |
| Tab | Switch front weapon slot |
| / | Switch rear weapon slot |
| Esc or P | Pause/resume, outside the race flyby |
| Left / Right, then Enter | Choose an action in the pause menu |

<a id="keyboard--menus-shops-and-saved-games"></a>

### Menus, shops and saved games

| Context / input | Action |
| --- | --- |
| Mouse movement / left click | Point to and activate controls |
| Arrow keys | Navigate menu entries; Left/Right adjust selected values/sliders |
| Enter; Space where offered | Activate selected item |
| Esc | Back/cancel; closes the full F10 options back to its original scene |
| Car shop: Left / Right | Previous / next car |
| Car shop: Enter | Select/buy car and continue to equipment |
| Equipment shop: Left / Right | Previous / next item |
| Equipment shop: Enter | Buy/sell selected item |
| Equipment shop: Up / Down | Increase / decrease repair amount |
| Equipment shop: R | Apply the selected repair |
| Save/load: Up / Down | Select save slot |
| Save/load: L or Enter / S | Load / save selected slot |
| Replay list: Up / Down | Select replay slot |
| Replay list: Enter / D | Play / delete selected replay |
| Replay playback: Space | Pause/resume |
| Replay playback: Left / Right | Seek backward / forward five seconds |
| Replay playback: Esc or Enter | Leave playback |
| Cinematic: Space, Enter or left click | Skip current clip |
| Cinematic: Esc | Skip remaining queued clips and continue |
| Name entry: letters, Space, Backspace | Edit name; Enter accepts, Esc leaves |
| Extras: Up / Down, Page Up / Page Down, wheel | Navigate/scroll cinematic list |

<a id="keyboard--model-viewer"></a>

### Model viewer

Open **Debug / Cheats > Extras > Model viewer**. These keys are contextual;
they do not replace the global F1-F12 shortcuts.

| Input | Action |
| --- | --- |
| Left / Right | Previous / next model |
| Up / Down or Page Up / Page Down | Previous / next model animation step |
| Left mouse drag | Rotate view |
| Wheel or + / - | Zoom |
| Space | Toggle automatic rotation |
| R | Toggle the viewer's additional spin animation |
| L | Cycle available levels of detail |
| W | Toggle wireframe |
| B / G | Toggle background / ground |
| H | Toggle help |
| Home | Reset view and reload the selected model |
| Esc | Return to the game |

The developer-only standalone video mode (`--mode fmv`) additionally supports
Left/Right for previous/next video, Enter to restart, Space to pause/resume,
M to mute/unmute and H for help. Extras cinematics instead use the normal clip
controls above. Old global viewer/debug mode-jump keys are removed.


---

<a id="recording"></a>

## Screenshots and recordings

<a id="recording--screenshots-and-recordings"></a>


F11 starts or stops a recording in every game view. Files are stored under
`revived_config/recordings/openmr2_*.mp4`. The recorder captures the presented
game picture and mixed game audio, including their current volume settings.
It does not capture the desktop, microphone or other applications.

The recording uses H.264 (libx264, veryfast preset, CRF 21), a maximum video
rate of 8 Mbit/s with a 16 Mbit buffer, and stereo AAC at 128 kbit/s. It keeps
the window's aspect ratio and caps resolution at 1920x1080; smaller windows
retain their resolution. The output is 30 fps, with repeated source frames
when Original mode renders below 30 fps. H.264 uses yuv420p for broad player
compatibility. The final MP4 moves its metadata to the beginning for uploads
and web playback. File size depends on content and duration, not a promise of
fitting every Discord account's upload limit.

Resize the window freely during recording: the initially selected recording
size remains fixed, with black margins when the new aspect ratio differs.
The recorder does not change simulation timing or the chosen graphics mode.
Readback and encoding add work while recording; inactive recording performs
no framebuffer readback or encoding. Encoding runs in a separate FFmpeg process;
its input pipe applies backpressure if encoding cannot keep up. On slower
machines, record a smaller window for lower overhead.

F12 writes a JPEG at **quality 98**, without chroma subsampling, under
`revived_config/screenshots/openmr2_*.jpg`. Images retain window resolution,
orientation and margins. Existing completed captures are never overwritten.
Explicit developer `--shot path.bmp` outputs remain lossless BMP for pixel tests.

Short translated messages appear at the top right when a recording starts or
stops, or a screenshot is saved. They disappear after three seconds. These
messages are drawn after capture and are not burned into the MP4 or JPG.

<a id="recording--encoder-installation"></a>

### Encoder installation

Windows looks first for `tools/ffmpeg.exe` beside OpenMR2.exe, then for
`ffmpeg.exe` through the normal executable search path. This local development
handoff includes the Gyan essentials build linked from FFmpeg's download page.
Its license and build README accompany the executable in `tools`. The encoder
is launched hidden and receives argument strings without a command shell.

Linux uses `ffmpeg` from PATH, or the exact executable named in
`OPENMR2_FFMPEG`. Install your distribution's FFmpeg package with libx264, for
example `sudo apt install ffmpeg` on Ubuntu. The AppImage uses the host encoder;
it does not silently download an encoder at runtime. FFmpeg is not required for
screenshots or ordinary gameplay. A missing encoder produces a visible message.

The separate GitHub release ZIP preparation does not bundle FFmpeg on either
platform. The bundled Windows encoder described above belongs to earlier local
development handoffs. Release-ZIP users install an encoder separately.

Stopping or normally closing the game finalizes video, adds mixed audio and
closes the game-owned encoder. Temporary `.video.mp4` and `.audio.s16` files
are removed after successful finalization. On an encoder/disk failure, those
files and the `.mp4.log` are retained for diagnosis; an interrupted process or
power loss is not guaranteed to leave a playable temporary video. Restarting
the game never deletes earlier recordings or recovery files automatically.

FFmpeg reference: [command line documentation](https://ffmpeg.org/ffmpeg.html),
[codec options](https://ffmpeg.org/ffmpeg-codecs.html), and
[download provenance](https://ffmpeg.org/download.html).

<a id="recording--global-function-keys"></a>

## Global function keys

| Key | Action |
|---|---|
| F1 | Switch Revived / Retro Mode |
| F2 | Toggle Revived texture filtering, including backgrounds |
| F3 | Switch Original / 60fps in Revived |
| F4 | Toggle custom textures in Revived |
| F5 | Cycle the Retro output filter |
| F6 | Cycle Retro scaling 1x–4x |
| F7 | Toggle experimental video upscaling in Revived |
| F8 | Cycle FSR / HQ4x / 4xBR / Lanczos3 / FSRCNNX AI / FSRCNNX AI HQ in Revived |
| F9 | Mute or restore all six Revived effect groups together |
| F10 | Open the full mode-specific options; Apply/Esc returns to the same scene |
| F11 | Start/stop MP4 recording |
| F12 | Save a quality-98 JPG screenshot |

Unavailable modern controls remain unavailable in Retro. F9 retains each
Revived effect checkbox and intensity instead of resetting them. Fullscreen is
available through its menu setting (or Alt+Enter on Windows), leaving F11 for recording.
The old global viewer/debug mode shortcuts are removed. Use
**Debug / Cheats > Extras** for the model viewer and the game's cinematic list.
The list excludes the six race-background videos, includes track introductions,
and supports the wheel, arrows and Page Up/Down. Esc returns from a viewer.

F10 pauses the scene, video clock and its audio, preserving its current state.
The full options menu is used, including the correct Retro variant. Applying
changes restores that scene without restarting a race. Volume changes remain
effective on return. Global shortcut settings are saved in the normal CFG.

<a id="recording--strict-retro-mode"></a>

## Strict Retro mode

Modern effects are always disabled in Retro. The former effects Easter egg was
removed in 0.7.14; old `retro_surprise` settings are ignored and removed on save.
F9 mutes/restores only Revived effects and leaves their individual preferences intact.


---

<a id="revive-effects"></a>

## Revived Effects - 0.7.11

<a id="revive-effects--revived-effects---0711"></a>


Open **Advanced Graphics > Revived Effects** (German: **Erweiterte Grafik > Revived Effekte**).
These preferences apply to player and opponent vehicles. Retro hides the submenu,
keeps original rendering and preserves the saved Revived preferences for later.
All six switches default to enabled when no explicit setting exists.

| Switch / CFG key | Behavior |
|---|---|
| Real-time shadows / `real_time_shadows` | A 2048 x 2048 directional shadow map, updated from the actual interpolated car/object poses; filtered shadows on reconstructed road surfaces and other models |
| Vehicle lights / `vehicle_lights` | Yellow texture regions emit warm headlights; red regions emit neon rear lights; braking intensifies the rear lights. HDR bloom softens the glow. Short red-only trails follow previous world poses, preserving the actual shape and number of lights |
| Enhanced oil slicks / `enhanced_oil` | Original slick mesh, translucent oily surface, subtle thin-film color variation and highlights |
| Modern explosion effects / `modern_explosions` | Animated RGBA flame atlas with upward-flow distortion and smooth variant blending; smoke, sparks and real 3D debris. Debris flies briefly and fades after approximately 1.1-1.55 seconds; smoke expires within 3.6 seconds |
| Damage effects / `damage_effects` | Progressive mesh damage, engine smoke/fire, vehicle destruction and metal-contact sparks, as detailed below |
| Missile effects / `missile_effects` | A short launch pulse, animated compact exhaust flame and soft smoke trail for all three missile types, fired by players or opponents |

All keys use 0/1 and remain in `revived_config/openmr2_setup.cfg`.
Each of the six effects has an independent **0-200% intensity** slider. **100%**
is exactly the current tuned default, including brighter headlights/braking,
stronger rear-light trails and intense explosive-hazard bursts. Zero suppresses
that checked effect without changing physics. Left/Right changes intensity by
5%; mouse clicking/dragging adjusts it directly. Enter/Space toggles the selected
checkbox. **Reset** enables all six switches and restores every intensity to 100%.
Values persist across launches and are retained while Retro is active.

The six CFG keys are `shadow_intensity`, `light_intensity`, `oil_intensity`,
`explosion_intensity`, `damage_intensity`, and `missile_intensity` (0-200).
Shadow intensity changes opacity, light intensity changes emission/bloom,
oil intensity changes opacity, explosion/missile intensity changes particle
visibility, and damage intensity changes visual deformation and damage-particle
visibility. Damage thresholds and simulation damage remain unchanged.

Lamp sources use the **original indexed texture markers**, not a general RGB
red/yellow detector. Red markers require at least four directly connected pixels.
Their exact UV footprints are restricted to non-wheel, rear-body, rear-facing
surfaces and a view from behind. Yellow markers are restricted to front-body
surfaces and a view from ahead, including hood-mounted lamps. Running tail lights
are brighter, braking emission rises from 1.35 to 7.0, and warm-white headlights
use 4.5 before the user intensity multiplier. HDR bloom follows these sources;
it does not turn nearby paint or wheels into emitting geometry.

Short red trails use nine faded historical poses (up to about 0.13 seconds and
9 render-world units) and remain visible without braking. Both career car selection and
single-race car selection use the same emission masks, intensity and HDR glow.
Custom textures preserve the original lamp UV footprint: painting new red/yellow
body pixels does not create extra lights. Transparent replacement pixels do not
emit. The Blatt 96 has front markers but no original red rear markers; its bright
front lights remain active, without inventing rear lights from paint speckles.

Barrels and mines now have a rapidly expanding bright blast core, a brief ground
shock ring, secondary flame tongues, heavier smoke and **48 solid debris pieces**.
The debris travels farther, stays bounded and still fades within about 1.55 seconds.
The modern-explosion switch controls these visuals independently of damage effects;
Retro and disabled-modern-explosion behavior retain the original sprites. No blast
radius, damage, audio or replay-format values are changed by the visual profile.

<a id="revive-effects--missile-exhaust"></a>

### Missile exhaust

**Advanced Graphics > Revived Effects > Missile effects** is an independent switch,
enabled by default. The Advanced Graphics link uses an arrow, while the actual
on/off options within Revived Effects keep their checkboxes.

Each missile has a brief stronger ignition pulse followed by a small turbulent
white/yellow/orange jet aligned behind its model. The emitter follows the exact
interpolated missile pose and scales modestly for Kard, Boomer and Hi-Trax.
Smoke is sampled along the travelled world-space path at 120 samples/second;
it expands, drifts upward and fades within 0.65 seconds. It stays behind when a
missile turns. On impact, the jet stops immediately and existing smoke fades out.

Emitters are bounded (32 missiles, 4,096 smoke samples), skip repeated sampling
at the same simulation time, and use independent visual seeds. Slot reuse,
teleports, race resets and timeline rewinds cannot connect unrelated missiles.
The switch works independently of modern explosions and damage effects. Retro
suppresses the effect and preserves the preference. When the debug console is
enabled, a new exhaust emitter is reported in English; individual smoke samples
are not logged. Exhaust is cosmetic and does not change missile physics, damage,
collision sizes, sound playback or replay file formats.

<a id="revive-effects--damage-thresholds"></a>

### Damage thresholds

| Damage | Visual result |
|---|---|
| Below 70% | Original intact shape; no damage smoke/fire |
| 70-79% | Light dents/deformation and white engine smoke |
| 80-84% | Clearly visible damage, still recognizable as a driveable car; white smoke |
| 85-89% | Increasing but bounded deformation; black smoke |
| 90-99% | Black smoke and animated flames. Flame size/intensity grows with each percentage point; deformation continues gradually while preserving the vehicle silhouette |
| 100% | One large explosion with the original large explosion sound, short debris flight and a strongly crushed/charred wreck; stronger sustained flames |

The damage switch controls the vehicle's death explosion/sound independently of
modern weapon explosion effects. Turning damage effects off suppresses Revived
vehicle death visuals/audio and restores the undeformed render model. Retro keeps
its original destruction behavior. A destroyed vehicle cannot repeatedly trigger
the death sound or explosion on subsequent damage calls.

Deformation happens on model vertices in the renderer; it never overwrites the
original model files or changes save-game formats. The damaged geometry is also
used by the shadow and light-mask passes. Engine emitters use the current car's
bounds and pose; their anchors are approximations of the engine/exhaust area,
not newly discovered original per-model engine sockets.

Sustained grinding at the road boundary produces a dense, speed-dependent
backward shower of hot metallic sparks. A short wall impact produces a separate
brief burst, without enabling the continuous grinding emitter. Vehicle contact
sparks originate from the collision contact point and depend on relative motion;
stationary overlapping cars do not emit an endless shower. This also applies to
opponent-to-opponent contact. Spark bursts are rate-limited; individual particles
expire within 0.56 seconds. Disabling damage effects or entering Retro suppresses
these particles. No collision impulses or damage calculations are changed.

<a id="revive-effects--renderer-limits"></a>

### Renderer limits

The original scenery remains prerendered video. Dynamic shadows can be received
by the reconstructed road surface and rendered models; unavailable scenery meshes
cannot receive arbitrary accurate dynamic shadows. This is a directional shadow
map with soft filtering, not ray tracing. Fire and smoke combine camera-facing
particles and shaders; debris is solid 3D geometry. This is not a fluid simulation.
Historic replay files do not contain full damage or brake histories; those
unrecorded states cannot be reconstructed accurately from old recordings.

<a id="revive-effects--related-gameplay-fixes"></a>

### Related gameplay fixes

- Retro scaling honors the selected 1x-4x maximum both with and without integer
  scaling. If the selected size does not fit, it is reduced to fit; integer mode
  prefers whole factors, and small windows keep a safe fractional fallback.
- Scraping the barrier plays the original `SFX/FROTTE2.WAV` loop and stops on
  leaving contact, pausing, ending the race or muting effects.
- Kard uses `TR_MIS2.M3D`, Boomer `TR_MIS1.M3D`, Hi-Trax `TR_MIS.M3D`, from each
  world's CARS folder. Their authored size and ground-relative flight height are
  preserved. `MISSI0.M3D` is no longer substituted for all three projectiles.
- Missile collisions sweep the complete flight segment. Explosive barrels and
  mines can be shot, including mines not yet armed. The nearest impact is chosen;
  oil is not explosive. Road connectivity and height tests protect other levels
  and branches. Nearby explosive barrels/mines can chain-react in a bounded queue.

<a id="revive-effects--assets-and-validation"></a>

### Assets and validation

The generated fire source, RGBA runtime atlas and complete Imagegen prompt are
in `assets/effects/README.md`. `tools/build_effect_asset.py` regenerates the
embedded header without network access. No original game texture is replaced
on disk. The atlas is embedded in the executable and requires no separate install.

`tests/gameplay_graphics.inc` checks settings, scaling, original missile models
across all six worlds and three languages, sweeps, detonations, scrape audio,
all eight vehicle deaths, particle lifetimes and RNG isolation. The companion
`tests/gameplay_graphics.py` captures actual OpenGL output for effects, threshold
states, opponent effects, trails, Retro isolation and expiration.


---

<a id="video-upscaling"></a>

## GPU video upscaling (Revived only)

<a id="video-upscaling--gpu-video-upscaling-revived-only"></a>


Advanced graphics provides a Video upscaling (experimental) checkbox and a separate filter
selector. It processes decoded cinematics and track-background videos before
camera reprojection/composition. HUD graphics, fonts, static menu backgrounds
and vehicle textures use their own rendering paths. The setting is unavailable
and hidden in Retro; the selected Revived preference is retained.

`video_upscaling = 0/1` enables the feature. Fresh configurations default to off
with FSR selected. Existing filter indices and saved choices remain valid:

| `video_filter` | Menu name | Processing |
| --- | --- | --- |
| 0 | FSR | FSR1 EASU reconstruction and RCAS sharpening |
| 1 | HQ4x | HQx pattern smoothing at 4x |
| 2 | 4xBR | xBR-lv2 edge smoothing at 4x |
| 3 | Lanczos3 | Separable windowed-sinc reconstruction |
| 4 | FSRCNNX AI | Learned 2x reconstruction, 8 channels and 14 neural passes |
| 5 | FSRCNNX AI HQ | Learned 2x reconstruction, 16 channels and 26 neural passes |

- FSR uses AMD's original MIT-licensed FSR1 EASU spatial reconstruction followed
  by RCAS sharpening. At large scale ratios, two 2x EASU stages precede RCAS.
- HQ4x uses the upstream two-pass HQx implementation with its genuine 4x lookup
  table. Its pattern classification can make video imagery look more like
  smoothed pixel art. The shader sources are LGPL-2.1-or-later.
- 4xBR uses Hyllian's MIT-licensed xBR-lv2 shader at a 4x scale. It is a pixel-art
  edge smoother, not a neural restoration model.
- Lanczos3 uses two separable six-tap windowed-sinc passes. This offers a more
  conventional reconstruction of photographic footage.
- FSRCNNX AI uses igv's normal-footage model from release 1.1. It reconstructs
  luminance with a compact convolutional neural network trained with slight
  compression distortions. The HQ option uses a larger network at greater GPU
  cost. Neither option uses the LineArt model, which can give natural footage
  an unwanted painted appearance.

The neural pipeline uses OpenGL 3.3 fragment passes, with signed RGBA16F feature
and residual textures. It reconstructs gamma-coded BT.601 luma and combines it
with the original scaled chroma. Mild, bounded detail sharpening and local
anti-ringing limits restrain halos. The CNN runs once at the source resolution
to produce a 2x image; larger working images use separable Lanczos enlargement
of that luma rather than another CNN stage. The final working image is capped
at 2048 pixels per dimension and checked against the device texture limit.

All filters run on the existing OpenGL GPU. Neural weights are embedded; no
CUDA, TensorFlow installation, separate inference runtime or model download is
required. None of the filters generates frames or changes the source frame
rate. Repeated identical frames produce deterministic results on the same
rendering path. These are spatial filters without temporal reconstruction:
compression noise, palette dithering and source aliasing may still shimmer.

Four bounded per-texture caches retain independently decoded video sources.
An unchanged source frame reuses its filtered result instead of running the
pipeline on every display refresh. Reopening a video invalidates its previous
cached output. Neural scratch textures are shared; each source retains its
own final RGB image. Disabling upscaling or entering Retro bypasses all these
passes, including neural inference.

FSR and Lanczos use 2x/4x working images with a 2048-pixel soft limit. Failed
shader compilation or allocation falls back to original sampling. FSRCNNX first
falls back to Lanczos3, then original sampling if that also fails.

FSR's GL3.3 shader adapter requests ARB_shading_language_packing and
ARB_gpu_shader5. If these extensions are unavailable, FSR falls back to the
original video sampling; the other filter choices remain independent.

There is no universally best filter for all of this game's video material.
FSR emphasizes reconstructed edges, Lanczos3 is a useful alternative for natural
footage, and HQ4x/xBR intentionally smooth pixel patterns. FSRCNNX adds learned
detail reconstruction, with the 8-channel option offering a lower GPU cost than
HQ. It cannot recover an exact high-resolution original from the compressed
source. Compare moving footage when choosing a filter; extra apparent sharpness
can also emphasize artifacts. Performance depends on the GPU and source size.
Synchronous timing is restricted to the explicit developer `--graphics-test 16`
capture path, not normal gameplay.

Upstream references and complete source/license texts:
[AMD FSR1](https://gpuopen.com/fidelityfx-superresolution/),
[HQx preset](https://github.com/libretro/glsl-shaders/blob/master/hqx/hq4x.glslp),
[xBR-lv2](https://github.com/libretro/glsl-shaders/blob/master/xbr/shaders/xbr-lv2.glsl),
[FSRCNNX release 1.1](https://github.com/igv/FSRCNN-TensorFlow/releases/tag/1.1),
and the [FSRCNN research paper](https://arxiv.org/abs/1608.00367).

FSRCNNX is Copyright (C) 2017-2021 igv, LGPL-3.0-or-later. The unmodified model
sources, full LGPL/GPL license texts and provenance hashes are included under
`src/third_party/fsrcnnx/`. The other filter sources are under
`src/third_party/fsr1/` and `src/third_party/video_filters/`. Release ZIPs carry
the third-party sources/notices; the matching source archive includes the
complete integration and offline adapter generators.

Rebuild adapters with `tools/build_video_fsr.py`, `tools/build_video_filters.py`
and `tools/build_video_cnn.py`; no network access is needed. Verify the neural
adapter with `python tools/build_video_cnn.py --check --self-test`.
`tests/video_cnn_gpu.py` checks real GL3.3 compilation, subpixel ordering, source
sampling and signed feature retention. These checks validate integration;
they do not establish subjective picture quality or a frame-rate guarantee.

Offline video export/replacement import is deferred in TODO.md. It needs exact
frame/audio timing and track-camera mapping; it is not part of this filter option.


---

<a id="physics"></a>

## Driving and AI - 0.7.17-dev

<a id="physics--driving-and-ai---0717-dev"></a>


<a id="physics--active-implementation"></a>

### Active implementation

Both modes now use the same reconstructed fixed-point driving and contact
chain. The earlier empirical steering gains and automatic road-curvature
assistance are removed. Revived rendering does not select different physics.

The original uses integer two-dimensional road frames for gameplay, separate
from the three-dimensional road used to place models over the track video.
The port reads the stored PST chord directions, lengths and frame rotations,
unfolds nearby actors into shared frames, and preserves the original traversal
order at forks and merges. Recomputing these values from the visual road, or
merging duplicate unfolded nodes, changes rounding and the steering feedback.

Each step stages all vehicle movements before resolving contacts. The original
five-circle hull, quantized distance lookup, contact-time search, contact order,
velocity retention, angular response and separation are then applied, followed
by road reprojection and the heading-dependent wall response. There are at most
sixteen contact resolutions per group, as in the executable. Defensive bounds
prevent pathological zero-separation cases from hanging the port.

The simulation retains its 40 ms step; 60 FPS interpolates presentation.
The removed subdivided-physics checkbox and Retro effects Easter egg remain
removed. Impact one-shots require a substantial collision and vary among the
original CHOC1/CHOC3/CHOC5 samples without immediate repeats. Light wall grinding
uses the separate FROTTE2 loop. Revived sparks do not affect the physics RNG.

<a id="physics--wheel-animation-0717"></a>

### Wheel animation (0.7.17)

The 0.7.16 physics integration advanced the original wheel state but did not
pass it to the renderer, leaving race wheels stationary. Both modes now use
the original signed increment, `(motor * 21 * dt) >> 16`, in 1,024 units per
turn, converted to radians for the wheel's X-axis rotation. Forward speed
increases rotation rate, reverse changes its direction, and stopped or retired
cars keep their last phase. This does not change driving or contact behavior.

The render phase remains unwrapped: a fast wheel can advance more than half a
turn per update. The 60 FPS path interpolates that full rotation alongside the
car pose, avoiding a shortest-arc interpolation that would reverse or slow it.
Retro retains the original update cadence; replays store and restore the same
render phase. The original INTRUDER and HUNTER models have their wheel artwork
baked into a single body node, so they retain that limitation. The other twelve
vehicle models have separate rotating axle nodes.

Run the focused test from the source directory on Linux:

```sh
g++ -std=c++17 -O1 -fno-math-errno -Wno-unused-result tests/wheel_rotation.cpp src/media.cpp -o build/wheel_rotation
(cd build && ./wheel_rotation /path/to/owned/german/game)
```

All **954 checks** pass. They cover all fourteen models on all six tracks,
wheel-only transforms and fixed axle centres, player/opponent phases in both
modes, forward/reverse/stopped motion, high-speed interpolation, recovery,
retirement and a replay round trip. The existing 4,000-case original-executable
drive comparison also passes all nineteen state fields without differences.

<a id="physics--validation-against-the-executable"></a>

### Validation against the executable

The reference harness executes the locally owned German GOG M.EXE in Unicorn.
It exercises the original group builder, pilot, all-car drive update, movement,
car-contact solver, reprojection, wall response and road-list commit. It compares
these with the active runtime implementation, rather than testing formulas alone.
Ghidra decompilation and disassembly were used to understand the routines;
no original executable, disassembly or runtime snapshot is distributed.

- Six real tracks, eight cars, 1,200 updates per track: **57,600 vehicle states**.
- Eight integer fields agree at every sample: road node, lateral position,
  heading, steering, lateral/forward velocity, drive speed and along-road position.
- The first 75 updates include the rolling start on every track.
- Separate randomized contact comparison: **6,000 cases**, including 4,390
  contacts; no differences in contact selection/time or resulting body state.
- Earlier standalone drive and wall tests cover additional profiles, commands,
  slip/oil states and update durations; their evidence remains in docs/physics.

Reports: [full loop](docs/physics/full-loop-0716.json) and
[car contacts](docs/physics/car-contacts-0716.json).
The complete-loop test fixes pilot targets and excludes weapons, road objects,
retirement and higher-level opponent tactics. Its result is not a claim that
all gameplay or every possible trajectory matches the original.

<a id="physics--reproduce-the-complete-loop-comparison"></a>

### Reproduce the complete-loop comparison

Install Python's Unicorn package. Compile tests/original_race_probe.cpp with
C++17 alongside src/media.cpp on Linux, then run:

```sh
g++ -std=c++17 -O1 -fno-math-errno -Wno-unused-result tests/original_race_probe.cpp src/media.cpp -o build/original_race_probe
python tests/original_race_reference.py --game /path/to/owned/german/game --cli build/original_race_probe --output build/full-loop.json
```

The supplied installation must contain the original M.EXE, MEGARACE2.gog and
extracted game directories. Temporary PST fixtures are deleted after the test.
Use --wsl when invoking a Linux probe from Windows with Ubuntu-24.04.

The separate contact test uses tests/original_body_cli.cpp and
tests/original_body_reference.py with the same --game, --cli, --output and
optional --wsl arguments. It compares the original detector and response to
original_body.h with deterministic randomized inputs.

<a id="physics--remaining-differences"></a>

### Remaining differences

Higher-level opponent tactics, hazard decisions and recovery behavior still
contain reconstructed rules. Road-object contact and motion remain partly
approximate; the original normal velocity exchange and retention are used for
heavy objects, but the complete mixed vehicle/object solver is not yet covered
by the original-loop comparison. Damage accounting, weapons and arbitrary
reverse/fork sequences need broader original-state validation.

The former 0.7.14 failure showed why isolated kernel tests are insufficient.
Future changes to the drive/controller/road/contact chain must preserve the
full-loop comparison and be checked in the actual game. No claim of 100% DOS
physics or renderer equivalence is made. See RETRO_RENDERER.md for graphics.


---

<a id="retro-renderer"></a>

## Retro renderer: implementation and remaining differences

<a id="retro-renderer--retro-renderer-implementation-and-remaining-differences"></a>


<a id="retro-renderer--evidence-from-the-supplied-original"></a>

### Evidence from the supplied original

The existing read-only LE disassembly sets width 0x140 (320), height 0x190 (400) and viewport height 0x15e (350) in the mode initialization at disassembler addresses 0x320bd and 0x32124. The supplied DOSBox captures m_000.png through m_004.png show a 640x400 capture with doubled horizontal pixels and 25-line upper/lower margins around the 350-line footage. These support a logical 320x400 race grid with a 320x350 picture, rather than the port's previous 320x480 target. Original assets/captures are not redistributed.

The span loops around disassembler addresses 0x67e65 and 0x680ec advance packed texture coordinates with ADD/ADC, read a byte texture index, map it through a byte lookup table and write a byte framebuffer pixel. Other paths shift 16-bit fractional edge coordinates before writing spans. This supports affine texture stepping, integer raster positions and an indexed output. The GPU implementation reproduces the broad affine/pixel-grid behavior; it does not reproduce these assembly loops instruction for instruction.

<a id="retro-renderer--implemented"></a>

### Implemented

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

<a id="retro-renderer--not-yet-equivalent-to-the-dos-renderer"></a>

### Not yet equivalent to the DOS renderer

This remains an OpenGL rasterization path. Exact fixed-point transform overflow/rounding, near-plane clipping, scanline endpoint/fill rules, polygon ordering, source material/vertex shade-table interpretation and indexed transparency/shadow lookup behavior have not been reconstructed completely. RGB blending/depth testing still differ from the original byte-oriented renderer. The current layered RGB composition can contain more than 256 simultaneous colors even though each source is indexed and final channels retain six-bit DAC precision. An exact shared byte framebuffer, shade-table indices and layer palette reservations still require reconstruction; do not reintroduce a whole-frame nearest-video-palette approximation to claim a 256-color limit. Pixel-exact DOS shading for every model/material is not claimed.

<a id="retro-renderer--color-depth-research-0712"></a>

### Color-depth research, 0.7.12

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

<a id="retro-renderer--strict-retro-0714"></a>

### Strict Retro, 0.7.14

The former effects Easter egg is removed. Legacy configuration cannot enable
modern effects. F9 operates only in Revived. On pause, the entire race viewport
moves down with the original 80-row menu, including video, models and HUD.
Original canvas pixels retain straight-alpha nearest sampling; premultiplied
filtering for Revived never alters Retro's CPU framebuffer.

See PHYSICS.md for the reconstructed drive/controller/shared-road/contact chain,
complete original-loop validation and remaining approximations. Full visual and
simulation parity is not claimed.


---

<a id="building"></a>

## Building from source

<a id="building--building-0713-dev"></a>

### Building 0.7.13-dev

The authoritative code is `src/`. The flat `.cpp/.inc/.h` files outside `src/` in the inherited working folder are obsolete copies and are excluded from source packages. Keep `g_menu_head.inc`, `g_menu_mid.inc` and `g_menu_tail.inc` authoritative: build scripts concatenate them into `g_menu.inc`.

Python tooling requires Python 3.11 or newer (tested: 3.14 on Windows and 3.12 on Ubuntu). Optional visual inspection/comparison tools use Pillow (tested: 12.1.1). Neither Python nor Pillow is needed to run the compiled game.

<a id="building--windows-x64"></a>

#### Windows x64

Tested toolchain: LLVM Clang/LLD 23.1.3, PowerShell, Windows x64. A Windows SDK, Visual Studio and the MSVC C runtime are not required by the freestanding build.

```powershell
./build_windows.ps1
./tests/run_windows.ps1
```


Windows build output is `build/OpenMR2.exe`. Use `-LLVM` to override the LLVM bin directory. No original game data is required to compile.

<a id="building--linux-x86_64"></a>

#### Linux x86_64

Tested environment: Ubuntu 24.04 under WSL, GCC 13, X11 headers, Mesa/llvmpipe for graphics tests. Install `g++ libx11-dev libgl1 libgl1-mesa-dri libpulse0`; `xvfb` is useful for noninteractive graphics tests. System package versions and bundled library hashes are recorded during packaging.

```sh
sh build_linux.sh
sh tests/run_linux.sh
python3 tools/fetch_appimage_tools.py
python3 tools/build_appimage.py
```

`build_linux.sh` accepts `CXX`. AppImage tools are downloaded from the official AppImage repositories; asset URLs and SHA-256 hashes are recorded in `build/tools/downloads.json`. The download script checks the upstream GitHub digest when supplied. AppImage output: `build/OpenMR2-0.7.13-dev-x86_64.AppImage`.

The AppImage build bundles transitive non-glibc dependencies, OpenGL dispatch/client libraries and PulseAudio client libraries. GPU-specific drivers remain system components. Its baseline is glibc 2.39; older distributions have not been verified. Public binary redistribution must wait for the license/corresponding-source audit described in `LICENSE_STATUS.md`.

<a id="building--original-data-tests"></a>

#### Original-data tests

The native regression suites use the actual original installations for file hashes, decoder frames/seeks, HUD sprites/palette and a ten-second Foundry simulation comparison at 25/60/240 Hz. Rendering is stubbed for those core comparisons. Audio tests include the original engine WAV and the real game-over transition, but do not replace a listening test.


`tools/generate_manifest.py FIXTURE_PARENT` regenerates SHA-256 fingerprints from all three fixtures. It is an explicit developer action, not a runtime detector: do not run it against damaged data. `tools/generate_localization.py` compiles the UTF-8 TSV into the bitmap renderer's single-byte strings. `tools/localize_builders.py` was the one-time literal UI migration helper.

Visual inspection tools `inspect_assets.py`, `inspect_executable.py` and `font_atlas.py` read original resources locally. Their output is not included in source/release ZIPs. See DISTRIBUTION.md and tools/package_github_release.py for separate source tarballs and platform ZIPs; original data, temporary builds and personal paths are excluded.

Fixtures: place Megarace 2 English, Megarace 2 French and Megarace 2 German beside the project directory. Runtime smoke scripts cover nine cases per language, including the quit dialog and both supported output modes. `python tests/render_pacing.py` compares rendered traces for the two modes.

Localization.tsv has stable key/de/en/fr columns. Run tools/generate_localization.py after editing; keys must remain unchanged. tools/audit_localization.py is a heuristic review aid, not proof that every screen has been inspected.

`tests/run_console_windows.ps1` builds a separate GUI test and exercises the actual menu checkbox, real Win32 console show/hide/history, persistence, English errors/events and protections against Quick Edit pauses / Close-command termination.


Custom-image integration: run tests/custom_textures.py with Python and Pillow (Ubuntu package python3-pil). This exports the complete English GOG static-image catalog into a private test directory (build/ on Windows, temporary storage on Linux), validates RGBA PNGs/material sidecars, renders one-pixel-detail backgrounds, models, partial-alpha UI/HUD and scaled font atlases, and checks pixel-identical original fallback/Retro suppression. It does not add original assets to release packages. The output is build/<platform>-custom-texture-results.json. Allow a few hundred MiB per export fixture.

Object palette capture checks: tests/object_palettes.py uses Pillow and renders OBJET2, OBJET1 and PLOT in all six world sets. White texture pixels may appear gray under the existing model lighting; native tests separately verify exact source-to-renderer RGB values.


The scalable Revived font is already embedded in src/revived_font_data.h.
Normal builds need no font package or system-font installation. To regenerate
the atlas, install Pillow, numpy and scipy, then run
`python tools/build_revived_font.py`. The source font, full SIL OFL license,
source URL and checksum are in assets/fonts/. Ship THIRD_PARTY_NOTICES.txt with
binaries; the font's full license is included there.


---

<a id="contributing"></a>

## Contributing

<a id="contributing--contributing"></a>


Edit the canonical implementation under `src/`. Build scripts generate
`src/g_menu.inc` from the head/mid/tail files; edit those parts instead.

Preserve original Retro graphics, fonts, timing and palettes. New visual
enhancements belong in Revived Mode unless a Retro change is explicitly agreed.
The former effects Easter egg is removed. Retro always bypasses modern effects.

New project-code contributions use GPL-3.0-only. Preserve upstream notices and
identify third-party code, generated assets and their provenance. Do not submit
decompiled/transcribed original executable code as independently authored code.
See LICENSING.md and the existing unresolved findings in PUBLICATION_AUDIT.md.

Do not commit game installations, extracted textures/audio/videos, credentials,
saves, personal config, screenshots or recordings. Build products belong in
`build/`; runtime output belongs in `revived_config/`.

Run the appropriate builds and targeted tests for changes. CI compiles without
original game files. Runtime and visual tests require privately supplied English,
French and German GOG fixtures; never upload these fixtures as CI artifacts.

For publication, keep README.md separate and concise. All other public Markdown
documents are combined into DOCUMENTATION.md by tools/prepare_github.py, with
section links adjusted automatically. Keep this layout in future source and
binary packages; preserve LICENSE and required plain-text notices separately.

For bug reports include the program version, OS/GPU, Revived/Retro mode, relevant
settings and reproduction steps. Redact personal paths and account details.


---

<a id="distribution"></a>

## Source and release layout

<a id="distribution--source-and-release-layout"></a>


The Git repository contains development source and documentation. Downloadable
assets are kept under GitHub Releases, separate from tracked source files:

- `OpenMR2-0.7.17-dev-source.tar.gz`: complete source snapshot, upstream source
  and license texts, tools, build scripts, documentation and graphics provenance.
- `OpenMR2-0.7.17-dev-Windows-x64.zip`: Windows executable and runtime documents.
- `OpenMR2-0.7.17-dev-Linux-x86_64.zip`: native Linux executable and runtime
  documents; uses host libraries, without bundled AppImage dependencies.
- `SHA256SUMS.txt`: SHA-256 hashes of the three downloadable archives.

These ZIPs do not contain original game data or FFmpeg. Users provide their
own GOG installation and optionally install an FFmpeg build supporting libx264
and AAC. Linux runtime packages are documented in the README.

The source tarball has one versioned top-level directory. Binary ZIPs have
separate platform-specific top-level directories and include LICENSE,
DOCUMENTATION.md, README.md, THIRD_PARTY_NOTICES.txt and a
SOURCE.txt identifying the exact accompanying source archive and its hash.

<a id="distribution--maintainer-workflow"></a>

### Maintainer workflow

1. Clone the existing repository into a separate directory.
2. From the working source tree, run `python tools/prepare_github.py --export
   PATH_TO_CLEAN_CLONE`. This preserves existing history and LICENSE and never
   stages, commits or uploads files. It refuses a destination with local changes.
3. Review `publication-manifest.json` and the [publication audit](#publication-audit). After intentional
   source edits, refresh the inventory with `python tools/prepare_github.py
   --refresh .` in the export repository and review the resulting diff.
4. Run `python tools/prepare_github.py --check .`, both platform builds and
   appropriate local tests. CI compiles without original game fixtures.
5. Create the tarball/ZIPs with `tools/package_github_release.py`, passing the
   explicit source repository, tested Windows/Linux executables and output path.
6. Push the reviewed commit and attach the separate archives to the matching
   GitHub prerelease. A development version is a prerelease, not a stable release.

No automatic deployment or binary upload occurs in the build workflow. Archive
creation is a technical packaging action, not clearance of unresolved decoder
or artwork redistribution rights. See [license status](#license-status) and the [publication audit](#publication-audit).


---

<a id="licensing"></a>

## License scope

<a id="licensing--license-scope"></a>


OpenMR2's independently created project code is licensed under the GNU General
Public License version 3 only (GPL-3.0-only), to the extent the contributors hold
the applicable rights. The repository's LICENSE contains the full license.
Contributions to project code use the same terms; contributors must have the
rights to submit them.

| Component | Terms / status | Notices and provenance |
| --- | --- | --- |
| Independently created project code and tools | GPL-3.0-only, within contributor-held rights | LICENSE and owner statement in LICENSE_STATUS.md |
| `src/ubbdec.inc` | Direct original-executable translation; redistribution rights unresolved | Host comment in `src/media.cpp`, address labels and CPU operations |
| `assets/revived_ui/` and `src/remastered_ui_data.h` | AI edits of original artwork; no blanket GPL grant for original artwork | Asset README, prompts, catalog and generated source sheets |
| Original game executable, disc images, graphics, videos and audio | Proprietary; externally supplied by the user | Absent from source export |
| Project fire atlas and `src/fire_atlas_data.h` | GPL-3.0-only to the extent contributors hold rights | Text-only generation prompt in `assets/effects/README.md` |
| Rajdhani font, SDF atlas and embedded font header | SIL OFL 1.1 | `assets/fonts/OFL.txt`, source URL/hash |
| stb image headers | MIT / public-domain dual license | Header notices and THIRD_PARTY_NOTICES.txt |
| AMD FSR1 | MIT | `src/third_party/fsr1/LICENSE.txt` and provenance.json |
| HQ2x / HQ4x shaders and lookup tables | LGPL-2.1-or-later | `src/third_party/output_filters/` and `video_filters/` |
| xBR-lv2 shader | MIT | Upstream header in `src/third_party/video_filters/xbr-lv2.glsl` |
| xBRZ freescale shader | Contains MIT and GPLv3 portions | Full upstream header and XBRZ-COPYING.txt |
| Generated filter headers | Include the above third-party material under its original terms | Builders and upstream sources included |
| FFmpeg/libx264 | Optional external executable; exact build determines terms | Not bundled in source export; https://ffmpeg.org/legal.html |

Retain all upstream notices and license texts. Embedding an image, font or
shader in a C++ header does not erase its license or provenance.

A GPL file cannot grant rights that belong to the original game's rightsholder.
The decoder and derivative artwork findings remain unresolved. This tree must
not be described as a uniformly cleared GPL source distribution. See
PUBLICATION_AUDIT.md. No affiliation with or endorsement by the original game's
rightsholders is asserted.


---

<a id="license-status"></a>

## License and redistribution status - provenance updated 2026-10-19

<a id="license-status--license-and-redistribution-status---provenance-updated-2026-10-19"></a>


The owner has now confirmed that all port code through 0.6.4 was created for this project with Claude, rather than obtained from an unidentified third-party port. The existing repository at https://github.com/nex86/openmr2 already contains a GPL-3.0 license. Independently created project code is offered under GPL-3.0-only to the extent the contributors hold the applicable rights. This resolves the unidentified upstream-author question, but does not establish rights in expression translated from the original executable.

The GPL grant does not cover rights belonging to the original game's rightsholder. In particular, `src/ubbdec.inc` is described by `src/media.cpp` as a static translation of the closed-source game's decoder. It retains original instruction-address labels and translated CPU operations. Its redistribution rights remain unresolved; it must not be described as independently authored GPL code. See LICENSING.md and PUBLICATION_AUDIT.md for scope and concrete findings.

Existing binary ZIPs and AppImages are development handoff candidates. Their existence is not legal clearance. The separate GitHub source export excludes those binaries, FFmpeg, original installations and personal data. It retains unresolved source components explicitly identified in the audit rather than falsely presenting the snapshot as uniformly cleared. Original MegaRace 2 programs and assets retain their respective rights. The AI-edited HUD/icons and their embedded header are derived from original artwork; their redistribution rights are also unresolved. The fire atlas has separate text-only generation provenance. No permission from the original game's rightsholder is asserted.

The AppImage includes copyright/license notices for bundled libraries, common referenced GNU license texts, and an exact library/package/hash inventory under `usr/share/doc/OpenMR2/third-party/`. These cover components such as X11/XCB, OpenGL dispatch, compiler runtime libraries and PulseAudio dependencies. The AppImage runtime is obtained from the official AppImage project. The redistribution audit remains open: verify each binary's corresponding source, runtime/subcomponent notices, LGPL replacement rights and any other applicable conditions before publication. No unverified assertion that this audit is complete is made.

The build tools themselves are not shipped as runtime dependencies. Windows uses generated import-library stubs and operating-system DLLs; Microsoft system DLLs are not copied into the package.

Useful primary references: [GNU guide for applying a license to one's own software](https://www.gnu.org/licenses/gpl-howto.en.html), [GNU license FAQ](https://www.gnu.org/licenses/gpl-faq.en.html), [AppImage runtime repository](https://github.com/AppImage/type2-runtime). These references do not determine this port's ownership or grant rights to the original game.


---

<a id="publication-audit"></a>

## GitHub publication audit

<a id="publication-audit--github-publication-audit"></a>


Date: 2026-10-19. Target: https://github.com/nex86/openmr2.

<a id="publication-audit--established-provenance"></a>

### Established provenance

The project owner confirms the port through 0.6.4 was created with Claude for
this project. The unidentified upstream-author question is resolved by that
statement. It does not establish that every line is independent of the original
executable. Existing GitHub history and its GPL-3.0 LICENSE are preserved in
the local preparation clone.

The source export is a complete development snapshot. Unresolved files remain
identified; technical completeness does not mean all rights have been cleared.

<a id="publication-audit--p01-directly-translated-executable-code"></a>

### P01: directly translated executable code

`src/media.cpp` explicitly calls `src/ubbdec.inc` a static translation of the
original self-modifying x86 decoder. It retains original instruction-address
labels, registers, flag calculations, self-modifying operand locations and
dispatches. This decoder is used for cinematics and racing backgrounds.

Resolution requires establishing applicable redistribution permission/legal
basis or replacing it with an independently authored decoder of documented
provenance. A generic emulator loading the user's original executable is
another architecture to assess, including performance; it is not automatic
legal clearance. Renaming or relocating translated code does not resolve P01.

<a id="publication-audit--p02-artwork-derived-from-original-sprites"></a>

### P02: artwork derived from original sprites

`assets/revived_ui/README.md` records edit-mode AI transformations of original
HUD/icons: 49 selected sprites and 59 source/frame/palette identities. Both
PNGs and `src/remastered_ui_data.h` contain the resulting artwork.

Resolution requires applicable permission/legal basis, independently designed
replacement artwork, or exclusion of both the PNGs and embedded representation
from a public build while retaining runtime fallback to user-supplied originals.
The fire atlas is separate: its documented input was a text-only prompt.

<a id="publication-audit--p03-previous-binary-packages"></a>

### P03: previous binary packages

The source export excludes existing Windows ZIPs, FFmpeg and AppImages. Those
private artifacts need a separate audit of exact bundled components, matching
corresponding sources, notices and LGPL obligations before being advertised as
compliant public binary releases. Host-installed FFmpeg avoids distributing
that encoder binary; it does not resolve P01/P02 in OpenMR2 itself.

<a id="publication-audit--p04-remaining-behavioral-reconstruction"></a>

### P04: remaining behavioral reconstruction

Race/menu/track code includes original addresses, constants, tables and
algorithms. Matching functionality and file formats does not by itself establish
copied protected expression. These references are review evidence, not proof
that every such file is infringing or independently implemented.

<a id="publication-audit--technical-preparation"></a>

### Technical preparation

- Export canonical src, reviewed assets, tests, tools, textual import-library
  definitions, build scripts and project documentation.
- Exclude game installations, extracted fixtures, obsolete flat source copies,
  builds, compiled libraries/executables, personal configuration, saves,
  recordings, screenshots, old archives, logs and Python caches.
- Generate all Windows import libraries from `.def` files, including GDI32.
- Remove the private Imagegen path; accept `--generated-dir` or stored sheets.
- Keep upstream sources, licenses and hashes. Generate per-file SHA-256 and
  provenance classifications in publication-manifest.json.
- Add Windows/Linux compile CI without proprietary fixtures or release uploads.
- Check known credential patterns, private home paths and prohibited files.
  This targeted scanner is not a guarantee that no secret can exist.

<a id="publication-audit--external-requirements"></a>

### External requirements

The noninteractive GitHub write probe failed without credentials. An
authenticated GitHub integration or local Git login is needed to push. Do not
paste credentials into chat or store them in the repository.

P01/P02 cannot be settled by an AI declaring the project safe. Neither age,
a GPL file, AI involvement nor a successful build independently establishes
permission from the original game's rightsholder. This technical audit is not
a binding legal opinion.

Primary references:

- https://www.gesetze-im-internet.de/urhg/__69e.html
- https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:62010CJ0406
- https://www.gnu.org/licenses/gpl-howto.html
- https://ffmpeg.org/legal.html


---

<a id="changelog"></a>

## Changelog

0.7.17-dev (2026-10-19): restored speed-dependent race wheel rotation, reverse direction and smooth 60 FPS wheel interpolation without changing physics. Experimental Revived video upscaling now offers six GPU filters: FSR, HQ4x, 4xBR, Lanczos3, FSRCNNX AI (8 channels) and FSRCNNX AI HQ (16 channels). The two learned filters reconstruct luma at 2x before Lanczos scaling to the output size. Retro keeps its original video path. See PHYSICS.md and VIDEO_UPSCALING.md.

0.7.16-dev (2026-10-19): reconstructed the complete fixed-point pilot/drive/shared-road/car-contact chain from the DOS executable. All 57,600 controlled vehicle states match on six tracks; 6,000 separate contact cases also pass. Removed empirical steering/curvature gains and fixed excessive contact bounce. Strong impacts use varied original sounds; light touches stay quiet. Counterclockwise vehicle previews sit on the pedestal; Retro uses original shop font/colors and no model shadow, while Revived uses optional realtime shadows. Removed the DOS launcher from Linux. See PHYSICS.md for test scope and remaining approximations.

0.7.15-dev (2026-10-19): rolled back the rejected 0.7.14 driving/contact integration to the accepted 0.7.13 behavior in both modes. Six-track/eight-car rolling-start traces match the stable reference in all 3,600 samples. Restored contact cooldowns; collisions vary among three original samples without consecutive repeats. The static Game Over screen is now silent at the mixer state boundary and no longer starts SCREAM.WAV. Strict Retro and independent graphics/audio teardown fixes remain.

0.7.14-dev (2026-10-19): original fixed-point driving and wall-response kernels, verified against the DOS executable; shared update cadence in both modes; removal of invented contact kicks and random heavy-barrel impulses. Removed the Retro effects Easter egg and legacy physics toggle. Premultiplied filtered canvas eliminates dark transparent-edge fringes in Revived. Retro pause displaces the entire scene. Engine-bus teardown blocks stale Game Over loops. Remaining parity limits are documented in PHYSICS.md and RETRO_RENDERER.md.

<a id="changelog--changelog"></a>

### Changelog

<a id="changelog--0713-dev"></a>

#### 0.7.13-dev

0.7.13-dev (2026-10-19): global F1–F12 shortcuts, race-preserving mode-specific options, scrollable cinematic/model Extras, quality-98 JPEG screenshots and H.264/AAC MP4 recording with transient top-right notifications. Revived background filtering now includes original picture layers. Disabled arrows retain transparency. Retro keeps source palette colors instead of recoloring all layers through a video's changing palette. Shop notices use a small transparent overlay; the audio-off hint is beneath the sliders. Loading WAIT.16 music uses race volume even with menu music muted. The Retro-only surprise surprise! checkbox is the explicit opt-in effects-only exception; every other Retro lock is retained. See RECORDING.md.


<a id="changelog--0712-dev"></a>

#### 0.7.12-dev

Revived-only AI remaster of HUD, free-standing equipment symbols, the cursor and the eleven explicitly approved bottom shop tiles. Other embedded buttons/backgrounds remain original. Strong backward grinding sparks and separate short wall/car impact bursts. Native Retro output is constrained to the active 256-color DAC palette before optional output filters.

Experimental GPU video upscaling with FSR1 EASU/RCAS, true HQ4x LUT filtering, xBR-lv2 at 4x and Lanczos3; bounded source-frame caching, saved selector, hidden/bypassed in Retro. Video export/import is recorded for later.

Includes 0.7.11: original directional lamp masks with minimum four-pixel red clusters for all fourteen cars; brighter front/rear lamps, brake intensification and trails; career/single preview HDR glow; stronger mine/barrel blasts; six effect intensity sliders and exact-default reset; consistent Revived naming.


<a id="changelog--079-dev"></a>

#### 0.7.9-dev

Revive Effects submenu: dynamic shadows, texture-shaped headlights/rear lights with brake glow and short trails, translucent oil, animated textured fire/smoke/3D debris, progressive damage/wrecks and contact sparks for all vehicles. Fix selected fractional output scaling, original metallic scrape audio, missile flight models/native size/height, and swept hits on explosive barrels/mines. See REVIVE_EFFECTS.md.


0.7.8 additions: English native Windows/Linux startup-mode chooser with optional saved choice; GOG-only original DOS launcher; final-frame Retro Point/Linear/HQ2x/BRZ/Interlacing/CRT filters, hidden in Revived.


<a id="changelog--078-dev---2026-10-18"></a>

#### 0.7.8-dev - 2026-10-18

- Read all fourteen original car/engine assignments, four profile levels and the complete playback/gear curve directly from each verified original executable. Use TURBINE/M_2/MOT3 for the correct player/opponent car; preserve shared original profiles.
- Restore the 0.7.6 options layout and full-size original bitmap labels in Retro. Put independent volume sliders into Advanced sound, with coarse segmented pixel bars.
- Show only supported Retro graphics/debug settings. Suppress recovery, smoothed camera and Revived physics while preserving their saved Revived preferences; remove the Retro texture-dump action.
- Keep the compact scalable Revived options/menu path. Check all three languages for overlap at both menu sizes.

<a id="changelog--077-dev---2026-10-18"></a>

#### 0.7.7-dev - 2026-10-18

- Rearrange options into two compact columns, remove dark panels and keep every label in its column in DE/EN/FR and Retro/Revived.
- Add a shared scalable SDF typeface for runtime game text throughout Revived; retain original bitmap fonts in Retro. Include Rajdhani SemiBold OFL notices and a reproducible atlas generator.
- Replace audio toggles with five independent 0-100 percent sliders, including player/opponent engines; support mouse dragging and keyboard/controller adjustment.
- Load saved percentages before audio, skip disabled categories, release stopped caches, gate video PCM decoding at zero and preserve silent video timing. Migrate legacy Boolean settings.

<a id="changelog--076-dev---2026-10-18"></a>

#### 0.7.6-dev - 2026-10-18

- Substitute only explicit accent palette markers 248/249; preserve object white 250 and all other real colors. Check all 408 vehicle/object textures across the three editions and all six worlds.
- Make Dump textures a one-shot action. Repeated clicks retain the active job, leaving menus or changing viewer mode does not pause it, and old dump_textures config values no longer start exports.
- Reuse the track loading bar as a global export overlay, show completion briefly and play the Apply menu effect once at completion.
- Refresh unedited legacy dumps containing the previous palette mistake, preserving edited exports and all replacement PNGs.


<a id="changelog--075-dev---2026-10-18"></a>

#### 0.7.5-dev - 2026-10-18

- Add an eager, incremental original-texture catalog exporter to texture_dumps, preserving source paths, palette variants, SPR alpha/placement and M3D material metadata.
- Add PNG replacements in texture_replacements for models, backgrounds, UI, sprites, HUD and font atlases. Keep original geometry, UV scale, hit boxes and glyph metrics; compose high-resolution UI images as ordered GPU layers.
- Restrict replacements to Revived, preserve the preference in Retro and add English diagnostic events and a complete custom_textures.txt guide.


<a id="changelog--074-dev---2026-10-18"></a>

#### 0.7.4-dev - 2026-10-18

- Resolve all vehicle texture reserved colors (248..255) through PAL_RACE instead of source placeholder colors; preserve every ordinary palette color. Audit all 342 vehicle texture files across six worlds and three editions.
- Correct the horizontally mirrored shop/selection camera and increase turntable speed from 0.6 to 2.1 radians/second (3.5 times).
- Stop stale engine instances by sample identity, clear all mixer channels on Game Over and flush queued Windows/PulseAudio output. Preserve the finite Game Over entry effect.
- Capture all 14 cars in both graphics modes on Windows and Linux; test pixel palettes, turn speed, silent PCM after the entry effect and Windows queued-audio reset.

<a id="changelog--073-dev---2026-10-18"></a>

#### 0.7.3-dev - 2026-10-18

- Lock texture filtering, MSAA, FXAA and framerate to the Retro settings, gray disabled controls and skip them during keyboard navigation. Persist a separate Revived snapshot and restore it on return, including after restart.
- Correct the Retro race/replay target to 320x400 with 350 native video lines, preserve 640x480 menus and use nearest sampling throughout. Restore anisotropic/mipmap filtering correctly on mode changes.
- Add integer projected-vertex snapping, affine texture interpolation and a Retro material path without the added Revived lighting. Record the distinction between this GPU approximation and unimplemented original scan-converter/shading-table quirks.

<a id="changelog--072-dev---2026-10-18"></a>

#### 0.7.2-dev - 2026-10-18

- Verify each selected/local game directory once with an English native progress dialog. Persist Files_OK, verified directory and detected edition in the setup CFG; keep the receipt when saving settings. Cached starts check presence only, including the M.EXE loader. Explicit --validate still performs a full check.
- Rename the classic presentation to Retro Mode in all languages and use localized Scaling (Retro Mode) labels. Migrate the old scaling config keys.
- Log loaded graphics, models and videos, including track backgrounds, while excluding sound filenames. Remove periodic FPS output. Disable logging, history, file output and optional diagnostic work when the console is off.
- Add English transaction events for vehicles, shield upgrades, launchers, ammunition, repairs, payments and configuration changes.

<a id="changelog--071-dev---2026-10-18"></a>

#### 0.7.1-dev - 2026-10-18

- Add a persisted Debug-menu console checkbox. Windows keeps one game-owned console with preserved history and red errors; Linux enables detailed terminal output. Log state/track/video/audio/settings events in English, independently of UI language. Protect against console Close and Quick Edit pauses.

- Review 271 text keys in German/English/French. Use modern Antialiasing/Framerate terminology, fix empty-slot and saved-game wording, translate missing replay/help/error strings and repair nested track-name fragments. Separate stable keys from German display values; render user names without translation. Check every catalog entry in every language and measure key labels with the actual bitmap font.

- Correct quit-menu Exit/Reset labels and French Quitter/Réinitialiser; use Debug / Cheats and Débogage / codes consistently.
- Offer only Original and 60fps, with automatic intermediate poses in 60fps only. Migrate old unlimited/2 settings to 60fps and discard the independent interpolation toggle.
- Fix saved game-directory lookup: the old case-folding operation changed underscores, preventing recognition of game_directory. Use a shared bounded parser for startup/settings, including quoted paths and UTF-8 BOMs.
- Prefer complete local game data, otherwise use the validated saved path; prompt only when both fail. Save the selected absolute path. Always show the missing-data introduction in English; cancellation exits normally.

<a id="changelog--earlier-070-work--2026-10-18"></a>

#### Earlier 0.7.0 work — 2026-10-18

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

<a id="changelog--0710-dev"></a>

#### 0.7.10-dev

- Dedicated Revive missile effects: brief launch pulse, animated exhaust jet and bounded world-space smoke trails for Kard, Boomer and Hi-Trax, including opponent shots.
- Preserve smoke briefly after impacts; reset unrelated emitters on slot reuse and timeline changes.
- Use an arrow for the Revive Effects submenu link in Advanced Graphics. Add a localized independent Missile effects switch to the submenu.

<a id="changelog--0711-dev"></a>

#### 0.7.11-dev

- Use original lamp markers, minimum four-pixel red clusters, body location,
  face direction and non-wheel nodes to prevent red paint/wheel emission.
- Brighter warm-white front lamps, stronger running rear-light trails and
  substantially brighter braking. Correct front lamps remain active on Blatt 96.
- Shared HDR lamp glow in career and single-race vehicle previews.
- Intense barrel/mine blast cores, shock rings, smoke and 48 brief solid debris pieces.
- Six 0-200% effect sliders, tuned 100% defaults, Reset and persistent settings.
- Visible branding consistently uses Revived, including Revived Effects and startup choice.


---

<a id="release-notes"></a>

## Release notes

0.7.17-dev (2026-10-19): restored speed-dependent race wheel rotation, reverse direction and smooth 60 FPS wheel interpolation without changing physics. Experimental Revived video upscaling now offers six GPU filters: FSR, HQ4x, 4xBR, Lanczos3, FSRCNNX AI (8 channels) and FSRCNNX AI HQ (16 channels). The two learned filters reconstruct luma at 2x before Lanczos scaling to the output size. Retro keeps its original video path. See PHYSICS.md and VIDEO_UPSCALING.md.

0.7.16-dev (2026-10-19): reconstructed the complete fixed-point pilot/drive/shared-road/car-contact chain from the DOS executable. All 57,600 controlled vehicle states match on six tracks; 6,000 separate contact cases also pass. Removed empirical steering/curvature gains and fixed excessive contact bounce. Strong impacts use varied original sounds; light touches stay quiet. Counterclockwise vehicle previews sit on the pedestal; Retro uses original shop font/colors and no model shadow, while Revived uses optional realtime shadows. Removed the DOS launcher from Linux. See PHYSICS.md for test scope and remaining approximations.

0.7.15-dev (2026-10-19): rolled back the rejected 0.7.14 driving/contact integration to the accepted 0.7.13 behavior in both modes. Six-track/eight-car rolling-start traces match the stable reference in all 3,600 samples. Restored contact cooldowns; collisions vary among three original samples without consecutive repeats. The static Game Over screen is now silent at the mixer state boundary and no longer starts SCREAM.WAV. Strict Retro and independent graphics/audio teardown fixes remain.

0.7.14-dev (2026-10-19): original fixed-point driving and wall-response kernels, verified against the DOS executable; shared update cadence in both modes; removal of invented contact kicks and random heavy-barrel impulses. Removed the Retro effects Easter egg and legacy physics toggle. Premultiplied filtered canvas eliminates dark transparent-edge fringes in Revived. Retro pause displaces the entire scene. Engine-bus teardown blocks stale Game Over loops. Remaining parity limits are documented in PHYSICS.md and RETRO_RENDERER.md.

<a id="release-notes--draft-openmr2-0713-dev"></a>

### Draft: OpenMR2 0.7.13-dev

0.7.13-dev (2026-10-19): global F1–F12 shortcuts, race-preserving mode-specific options, scrollable cinematic/model Extras, quality-98 JPEG screenshots and H.264/AAC MP4 recording with transient top-right notifications. Revived background filtering now includes original picture layers. Disabled arrows retain transparency. Retro keeps source palette colors instead of recoloring all layers through a video's changing palette. Shop notices use a small transparent overlay; the audio-off hint is beneath the sliders. Loading WAIT.16 music uses race volume even with menu music muted. The Retro-only surprise surprise! checkbox is the explicit opt-in effects-only exception; every other Retro lock is retained. See RECORDING.md.

Revived adds AI-redrawn HUD/symbol/cursor assets and eleven uniformly aligned shop tiles, stronger backward grinding sparks and short impact bursts. Embedded controls, main-menu artwork and portraits remain original. Experimental video upscaling offers FSR, HQ4x, 4xBR and Lanczos3 GPU filters; it is disabled by default and unavailable in Retro. Native Retro output uses the active original 256-color palette before optional output filtering. See VIDEO_UPSCALING.md, RETRO_RENDERER.md and assets/revived_ui/README.md for scope and limitations. Offline video export/import is deferred in TODO.md.

**Private development candidate. Not cleared for public release.**

This candidate restores the original track HUD icons and fine status bars, fixes race-engine loops persisting across game-over transitions, and introduces verified German/English/French GOG data recognition. Language-dependent video-decoder addresses are normalized so the tested original track frames decode consistently across all three data sets.


Only the German, English and French GOG editions are supported; original CD support is planned later. Bring your own game files. No original assets are included.

Before publication: resolve the source/decoder license, complete third-party redistribution compliance, finish localization and HUD typography/layout review, and perform interactive gameplay/audio/controller/clean-machine acceptance tests. Existing approximations in physics/AI/camera behavior remain. Linux controller support is absent; Linux fullscreen/audio need broader desktop testing. See TEST_REPORT.md and WORK_STATUS.md for exact evidence and outstanding items.

<a id="release-notes--072-changes"></a>

#### 0.7.2 changes

One-time local/selected-folder verification now shows an English progress dialog and writes Files_OK plus the verified path/edition to the setup CFG. Cached starts check presence only; use Files_OK=false to recheck changed contents. Retro Mode replaces the previous name in all languages. Console output reports graphics/models/videos and inventory/settings events in English, omits sound filenames and periodic FPS, and stops recording/formatting diagnostics while disabled.

<a id="release-notes--073-changes"></a>

#### 0.7.3 changes

Retro mode now locks modern graphics options while retaining the user's Revived settings across mode changes/restarts. Native race/replay resolution is 320x400. The Retro model path uses affine textures, integer pixel positioning and omits added modern lighting. Complete software-rasterizer parity remains open; see RETRO_RENDERER.md.


<a id="release-notes--074-vehicle-and-audio-corrections"></a>

#### 0.7.4 vehicle and audio corrections

All 342 vehicle texture files (six worlds, three editions) are decoded and checked pixel by pixel: reserved indices 248..255 use DATA640/PAL_RACE.HLZ, while all other colors remain unchanged. This fixes placeholder magenta/green replacing red/yellow accents for every vehicle texture set. The preview camera horizontal basis is corrected, and previews rotate 3.5 times faster.

Game Over now stops all mixer channels, clears stale engine instances and flushes queued output before the finite entry effect. Native PCM checks require silence after that effect. Windows additionally tests the actual transition through a mocked waveOutReset queue; no speaker-listening acceptance is claimed. All 14 cars are captured in both modes on each platform; see test-results.json for results.


<a id="release-notes--custom-textures-075"></a>

#### Custom textures (0.7.5)

Debug / Cheats > Dump textures exports original static assets and material references to texture_dumps. Advanced Graphics > Enable custom textures loads edited PNGs only from texture_replacements, keeping source-relative LOCAL/CD paths and palette-qualified SPR frame names. Replacements are suppressed in Retro while the Revived preference is retained. Higher-resolution UI is composed as ordered GPU layers; geometry, hit boxes and original UV normalization remain unchanged. English diagnostics describe exports, loads, alpha, fallback and cache reload. Read custom_textures.txt for complete instructions and limits. UBB video frames and procedural graphics are outside the static-image replacement feature.


<a id="release-notes--076-palette-and-export-action-corrections"></a>

#### 0.7.6 palette and export action corrections

Only explicit accent marker colors at palette indices 248/249 are replaced; all other entries, including white barrel skulls, are preserved. Dump textures is an action, with no saved toggle or cancellation on repeated clicks. Export progresses in every view, displays the same bar as track loading, stops automatically and plays the Apply sound once. Existing dump_textures config keys are ignored. Corrected original dumps refresh only exact, unchanged legacy outputs; edited exports/replacement PNGs are preserved.

Texture export/replacement folders are stored inside revived_config: revived_config/texture_dumps and revived_config/texture_replacements.


0.7.7 adds a compact options layout without dark panels, five independent volume sliders, zero-volume loading/playback gates and scalable Revived text throughout the game. Retro retains original bitmap fonts. See CONFIGURATION.md for audio migration and custom-font precedence.


0.7.8 restores the original per-car engine profiles and pitch curve, the old Retro options layout with original-size type, a separate Advanced sound menu with pixel bars, and a Retro-only graphics/debug list. See ENGINE_AUDIO.md and CONFIGURATION.md.


<a id="release-notes--startup-mode-and-original-dos-launcher-078"></a>

#### Startup mode and original DOS launcher (0.7.8)

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

On Windows with recognized GOG editions, the black-flag menu also offers **Run DOS Version**
(**DOS Version starten**, **Lancer la version DOS**). Unknown editions never
show this entry. Windows starts `DOSBOX/DOSBox.exe` from the selected game's
`DOSBOX` directory with the original shortcut arguments:

```text
-conf "..\dosboxmegarace2.conf" -conf "..\dosboxmegarace2_single.conf" -noconsole -c "exit"
```

OpenMR2 closes only after process creation succeeds. Missing executable/config
files or launch failure leave OpenMR2 running with an error message. Linux does
not show this menu entry or launch DOSBox.
Successful process creation cannot guarantee that DOSBox subsequently starts
the game successfully; its own configuration/errors still apply.

<a id="release-notes--retro-output-filters-078"></a>

#### Retro output filters (0.7.8)

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

<a id="release-notes--079-changes"></a>

#### 0.7.9 changes

<a id="release-notes--revived-effects---079"></a>

### Revived Effects - 0.7.9

Open **Advanced Graphics > Revived Effects** (German: **Erweiterte Grafik > Revived Effekte**).
These preferences apply to player and opponent vehicles. Retro hides the submenu,
keeps original rendering and preserves the saved Revived preferences for later.
All five switches default to enabled when no explicit setting exists.

| Switch / CFG key | Behavior |
|---|---|
| Real-time shadows / `real_time_shadows` | A 2048 x 2048 directional shadow map, updated from the actual interpolated car/object poses; filtered shadows on reconstructed road surfaces and other models |
| Vehicle lights / `vehicle_lights` | Yellow texture regions emit warm headlights; red regions emit neon rear lights; braking intensifies the rear lights. HDR bloom softens the glow. Short red-only trails follow previous world poses, preserving the actual shape and number of lights |
| Enhanced oil slicks / `enhanced_oil` | Original slick mesh, translucent oily surface, subtle thin-film color variation and highlights |
| Modern explosion effects / `modern_explosions` | Animated RGBA flame atlas with upward-flow distortion and smooth variant blending; smoke, sparks and real 3D debris. Debris flies briefly and fades after approximately 1.1-1.55 seconds; smoke expires within 3.6 seconds |
| Damage effects / `damage_effects` | Progressive mesh damage, engine smoke/fire, vehicle destruction and metal-contact sparks, as detailed below |

All keys use 0/1 and remain in `revived_config/openmr2_setup.cfg`.
Light trails are intentionally short (roughly 0.07 seconds, with a bounded world length),
not long permanent ribbons. Only red rear-light regions trail; headlights retain bloom.
Orange body accents are excluded from the yellow-light mask. Custom car textures
participate in the same color masks, so changing red/yellow pixels changes emission.

<a id="release-notes--damage-thresholds"></a>

#### Damage thresholds

| Damage | Visual result |
|---|---|
| Below 70% | Original intact shape; no damage smoke/fire |
| 70-79% | Light dents/deformation and white engine smoke |
| 80-84% | Clearly visible damage, still recognizable as a driveable car; white smoke |
| 85-89% | Increasing but bounded deformation; black smoke |
| 90-99% | Black smoke and animated flames. Flame size/intensity grows with each percentage point; deformation continues gradually while preserving the vehicle silhouette |
| 100% | One large explosion with the original large explosion sound, short debris flight and a strongly crushed/charred wreck; stronger sustained flames |

The damage switch controls the vehicle's death explosion/sound independently of
modern weapon explosion effects. Turning damage effects off suppresses Revived
vehicle death visuals/audio and restores the undeformed render model. Retro keeps
its original destruction behavior. A destroyed vehicle cannot repeatedly trigger
the death sound or explosion on subsequent damage calls.

Deformation happens on model vertices in the renderer; it never overwrites the
original model files or changes save-game formats. The damaged geometry is also
used by the shadow and light-mask passes. Engine emitters use the current car's
bounds and pose; their anchors are approximations of the engine/exhaust area,
not newly discovered original per-model engine sockets.

Friction sparks at the road boundary require actual sustained scraping while
moving. A hard wall bounce alone does not generate that effect. Vehicle contact
sparks originate from the collision contact point and depend on relative motion;
stationary overlapping cars do not emit an endless shower. This also applies to
opponent-to-opponent contact. Spark bursts are rate-limited and expire within half
a second. Disabling damage effects or entering Retro suppresses these particles.

<a id="release-notes--renderer-limits"></a>

#### Renderer limits

The original scenery remains prerendered video. Dynamic shadows can be received
by the reconstructed road surface and rendered models; unavailable scenery meshes
cannot receive arbitrary accurate dynamic shadows. This is a directional shadow
map with soft filtering, not ray tracing. Fire and smoke combine camera-facing
particles and shaders; debris is solid 3D geometry. This is not a fluid simulation.
Historic replay files do not contain full damage or brake histories; those
unrecorded states cannot be reconstructed accurately from old recordings.

<a id="release-notes--related-gameplay-fixes"></a>

#### Related gameplay fixes

- Retro scaling honors the selected 1x-4x maximum both with and without integer
  scaling. If the selected size does not fit, it is reduced to fit; integer mode
  prefers whole factors, and small windows keep a safe fractional fallback.
- Scraping the barrier plays the original `SFX/FROTTE2.WAV` loop and stops on
  leaving contact, pausing, ending the race or muting effects.
- Kard uses `TR_MIS2.M3D`, Boomer `TR_MIS1.M3D`, Hi-Trax `TR_MIS.M3D`, from each
  world's CARS folder. Their authored size and ground-relative flight height are
  preserved. `MISSI0.M3D` is no longer substituted for all three projectiles.
- Missile collisions sweep the complete flight segment. Explosive barrels and
  mines can be shot, including mines not yet armed. The nearest impact is chosen;
  oil is not explosive. Road connectivity and height tests protect other levels
  and branches. Nearby explosive barrels/mines can chain-react in a bounded queue.


<a id="release-notes--0710-dev"></a>

#### 0.7.10-dev

Revived missiles now leave a short fading smoke trail and have a small animated exhaust flame with a stronger ignition pulse. The independent **Missile effects** switch is under **Advanced Graphics > Revived Effects**. The submenu entry now uses an arrow. Retro retains its original presentation.

<a id="release-notes--0711-dev"></a>

#### 0.7.11-dev

Revived Effects now has six 0-200% sliders and Reset (100% restores the current tuned values). Vehicle lamps use original marker pixels and correct front/rear body locations, avoiding paint and wheels. Headlights/braking and running rear trails are stronger; lamps glow in both vehicle selections. Barrels/mines have large blast cores, shock waves, smoke and 48 flying debris pieces. The visible mode/menu name is consistently Revived.


---

<a id="todo"></a>

## Deferred work

<a id="todo--deferred-work"></a>


<a id="todo--video-export-and-replacement-import-requested-2026-10-18"></a>

### Video export and replacement import (requested 2026-10-18)

Provide a one-shot export of original cinematics and track-background videos,
with progress/cancellation, suitable for offline restoration or neural upscale.
Use lossless FFV1 in Matroska with original PCM audio, or an optional numbered
PNG-frame sequence plus WAV. Include a machine-readable sidecar with stable
LOCAL/CD source identity, original file hash, exact frame count and rational
frame rate, source resolution, display pixel aspect and color/palette metadata.

Define and validate the matching import contract before exporting: each decoded
replacement frame must map to exactly the same original frame number; keep
audio duration, seek behavior and the CAM/PST/V3D track-camera references intact.
Do not insert/drop frames or retime track footage. Prefer lossless intermediates
for editing; select an efficient, seekable runtime codec separately after testing.

Load optional replacements in Revived Mode only; keep the original UBB and
palette path in Retro. Add clear fallback for missing/invalid files and a limit
on replacement decoding memory. Export/import is not implemented yet.

The current runtime option uses GPU FSR1 EASU/RCAS spatial filtering. It is not
neural video restoration and does not invent true high-resolution source detail.

<a id="todo--original-dos-renderer-parity"></a>

### Original DOS renderer parity

Recover original shade-table selection, indexed shadow/transparency lookup,
scanline fill/clipping/rounding and polygon ordering. Compare matched camera,
vehicle, frame and palette states against DOSBox captures before claiming
pixel-exact rendering. See RETRO_RENDERER.md for confirmed evidence and limits.


---

<a id="engine-audio"></a>

## Original engine audio

<a id="engine-audio--original-engine-audio"></a>


The runtime reads car assignments, original profile levels and the complete
1,400-entry speed/playback-frequency curve from the user's verified M.EXE.
It does not synthesize fourteen different sounds or select a sample by vehicle
appearance. Original game audio files are required and are not bundled.

| Car | Original profile | Original sample | Original level |
|---|---:|---|---:|
| Thunder V. | 0 | SFX/TURBINE.WAV | 20 |
| Vipper | 1 | SFX/M_2.WAV | 20 |
| Astra B. | 2 | SFX/MOT3.WAV | 18 |
| Strata VII | 3 | SFX/M_2.WAV | 18 |
| CoCoon | 1 | SFX/M_2.WAV | 20 |
| BenHur 2000 | 2 | SFX/MOT3.WAV | 18 |
| MyWay | 3 | SFX/M_2.WAV | 18 |
| Traction XII | 1 | SFX/M_2.WAV | 20 |
| Futura IV | 2 | SFX/MOT3.WAV | 18 |
| Condor | 3 | SFX/M_2.WAV | 18 |
| Flyzz II | 1 | SFX/M_2.WAV | 20 |
| TonnCar | 2 | SFX/MOT3.WAV | 18 |
| Flyzz | 3 | SFX/M_2.WAV | 18 |
| Blatt 96 | 1 | SFX/M_2.WAV | 20 |

There are four profiles and three distinct samples; profiles 1 and 3 use the
same sample at different levels. Shared profiles are original behavior.
The engine volume slider applies to all profiles independently of sound effects.
The player and the audible nearby opponent use their own car's assignment.
Changing the audible opponent retires its former loop. Race teardown also
retires lost/stale loop handles for all three sample types.

<a id="engine-audio--read-only-evidence"></a>

### Read-only evidence

Addresses below refer to the loaded, relocated LE image, not raw file offsets.

| GOG edition | Car/profile table | Playback-frequency table |
|---|---:|---:|
| English | 0xDCFAC | 0x858B6 |
| French | 0xDD63C | 0x85F42 |
| German | 0xDD64C | 0x85F5A |

The profile table contains fourteen 32-bit indices. Immediately following it
are four records of 36 bytes: a 32-byte sample filename and a 32-bit level.
The model-name sequence in the executable confirms the same fourteen-car order.
English instructions at 0x45514..0x4553A select the player's sample through this
table; 0x4561B..0x456A3 apply its level and speed/frequency lookup. The lookup
uses half the magnitude in reverse. The curve has original gear drops at raw
speeds 300 and 600 and saturates at 22,050 Hz. All original engine samples used
here are 8-bit mono PCM at 22,050 Hz. The former MOTEURA/_MOTEUR2 fallback
samples are not the samples selected by this original table.

Run `python tools/inspect_engine_audio.py` against the three GOG fixtures.
It checks all 42 assignments, twelve profile records, sample metadata/hashes
and equality of all 4,200 pitch entries, writing only metadata to
build/engine-audio-research.json. It never modifies original files.

Native tests also load each edition and start real mixer voices for every car,
check correct sample/bus ownership, gear drops/reverse/clamping, opponent
sample changes and cleanup. The surrounding spatial mixer and driving model
remain the port's implementation; this work establishes the original sample
assignment and playback curve rather than full DOS mixer/physics equivalence.


---

<a id="hud-research"></a>

## HUD and decoder findings

<a id="hud-research--hud-and-decoder-findings"></a>


Research used the supplied original installations and screenshots. Referenced addresses below are the relocated German GOG LE image, exported read only by `tools/inspect_executable.py`; no original executable/asset dump is included in packages.

<a id="hud-research--icons-the-actual-error-was-resource-selection"></a>

### Icons: the actual error was resource selection

The old port decoded `DATA640/ARMES.HLZ` as a row-major 320x25 image, discovered groups of occupied columns, and assigned those groups to weapons/mine/oil/shield. The file really contains those elongated graphics; decoding it with alternate strides/planes or palettes does not recover the requested skull/shield. It is not the HUD resource selected by the original draw path.

The original path at 0x143b2/0x1440f selects the six frames loaded from the current track's `.SPR` file. The 0x13ca6 load uses the track stem plus `.SPR`. Frames are: 0 small missile, 1 medium missile, 2 large missile, 3 mine/skull, 4 oil, 5 shield. The Foundry frame dimensions are respectively 20x15, 17x12, 16x13, 14x23, 16x28, 20x22. Sprite records have a 24-byte header and tightly packed row-major pixels; index zero is transparent. These original pixels are used directly.

The associated track `.HLZ` supplies the palette. The original palette-loading helper replaces the filename extension with HLZ even where callers build a `.PAL` stem. A separate original load at 0x13e09 merges the five reserved colors 251..255 from `DATA640/PAL_RACE.HLZ`. In particular, 251 is black, 252 white, 254 red. Using only the track palette leaves status-bar colors black. No hand-drawn icon replacement, image-generation substitution or arbitrary red recoloring was used.

<a id="hud-research--layout-bars-and-values"></a>

### Layout, bars and values

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

<a id="hud-research--englishfrench-decoder-corruption"></a>

### English/French decoder corruption

The statically translated decoder references German data addresses. English strings shift the decoder-table block by -1696 bytes; French shifts it by -16 bytes. The old sanity check only required a pointer to fall within the decoder code range, so incorrect shifted entries could pass and still produce corrupt images.

All 23,124 relocated table bytes from 0x98220 to 0x9dc74 are identical across the three supplied versions after accounting for their offsets. The shared SHA-256 is `3ee0a5ed437793d45603b6f90d415683f1f825d5c8207f540da9a4f5d3b766e0`. Normalization copies this block in memory, backwards because ranges overlap, and adjusts the seven initial self-modifying operands at 0x25546, 0x26751, 0x285aa, 0x285ba, 0x29972, 0x29982, 0x2a9d6. Original installation files remain unchanged. The complete normalized block is verified before use, not just one pointer.

Actual scenery frames 0 and 150 and a backward seek to 30 now decode to identical RGBA hashes across all three languages. Initial broken renders were observed before the fix and are not counted as passes.


---

<a id="localization-review"></a>

## Localization review - 0.7.6-dev

<a id="localization-review--localization-review---076-dev"></a>


271 stable text keys were reviewed in German, English and French. Keys are separate from German display text, allowing natural phrasing without changing runtime identifiers. The original renderer uses Latin-1 with composed French accents. Translation lookup runs once before layout/drawing; player-entered leaderboard names use raw text rendering.

<a id="localization-review--wording-and-terminology"></a>

### Wording and terminology

- Quit menu: Exit / Reset; French Quitter / Réinitialiser. Abandoned-driver result text remains a separate context.
- Debug / Cheats, French Debug / Cheats, with consistent submenu headings.
- Antialiasing, Framerate, MSAA, FXAA, FPS and Integer Scaling are intentional technical labels. Regular help remains in the selected language.
- Game menu replaces Mark selection. Race results replaces Classification. Empty slot replaces the incorrect Do not save label; Load game and Current game describe the actual saved-game interface.
- Replay controls, model/video help, weapon descriptions and native Windows graphics errors are covered. Proper names and the original game's deliberate humorous fuel descriptions retain their identity.
- Composed track names in saved games/replays are translated before concatenation. User names and paths are preserved.

<a id="localization-review--exceptions-and-checks"></a>

### Exceptions and checks

The initial missing-data introduction is always English by request. Until game data can be recognized, English is the fallback language. Filenames, OpenGL identifiers, machine-readable traces and technical log field names remain unchanged. The font-test sample intentionally shows German and French characters together. Original videos, voice, imagery and credits are read from the selected language's GOG data; no replacement media were invented.

Native tests resolve every catalog key in each language and check important labels against their actual menu widths using the original bitmap font. Runtime captures cover all three language sets. Additional Windows captures inspect shops, saves, replays, single-race setup, results-related screens and fonts. See test-results.json for actual completed runs.

This is an editorial/code/layout review, not a claim of a full human gameplay acceptance pass or native-speaker certification. Rare error paths and all possible dynamic text lengths still need broader acceptance.

Debug console output and on-screen debug metrics deliberately use English in every language. The console checkbox itself follows the UI language; filenames and upstream driver output remain unchanged.


---

<a id="assets-effects-readme"></a>

## Vehicle fire asset

<a id="assets-effects-readme--vehicle-fire-asset"></a>


Created for OpenMR2 using the built-in Imagegen tool (transparent output).
`vehicle-fire-source-v1.png` preserves the full generated RGBA image.
`vehicle-fire-atlas-v1.png` is the 512 x 512 runtime atlas; its four cells
are blended and distorted by the flame-flow shader in `src/g_effects.inc`.
`tools/build_effect_asset.py` embeds the PNG in `src/fire_atlas_data.h`.
The alpha channel is preserved; no black color key is used.

The renderer uses softer, smaller flames from 90% damage, growing at each percentage point,
and broader flames on a wreck. All graphics are project assets, not extracted original game art.

<a id="assets-effects-readme--final-generation-prompt"></a>

### Final generation prompt

Use case: photorealistic-natural. Asset type: RGBA flame texture atlas for a real-time vehicle fire particle shader. Generate one square sprite atlas, a precise 2 by 2 grid of four distinct natural flame clusters, all isolated on genuinely transparent background. Each cell contains a full upright irregular cluster of realistic burning gasoline/vehicle fire, rooted at the bottom of its cell with curling orange flame tongues, hot pale yellow cores, dark orange wisps, intricate semi-transparent feathered edges. Keep a transparent gutter around every cell so nothing touches another cell or the atlas edge. Photographic fire texture, natural turbulent tongues and ragged silhouettes, no circular fireball, no solid disc, no halo ring, no neon outline, no smoke (smoke is rendered separately), no objects, no car, no lettering or grid lines. Flame roots low, tips upward; transparent gaps between individual tongues. These are four variations of one fire asset, not a scene.


---

<a id="assets-revived-ui-readme"></a>

## Revived HUD and standalone symbols

<a id="assets-revived-ui-readme--revived-hud-and-standalone-symbols"></a>


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


---

<a id="docs-validation"></a>

## Publication build validation

<a id="docs-validation--publication-build-validation"></a>


Validation on 2026-10-19 used the separate exported source tree, without
changing the installed development build or original game data.

| Check | Result |
| --- | --- |
| Windows x64 clean-source compile | Passed; all import libraries generated from .def files |
| Windows native regressions | 16,324 assertions, zero failures |
| Linux x86_64 clean-source compile | Passed on Ubuntu 24.04 |
| Linux native regressions | 16,314 assertions, zero failures |
| Actual Windows rendering | Six captures: race/options/car selection, both modes |
| Actual Linux rendering | Six captures: race/options/car selection, both modes |
| Export checker negative/positive cases | Eight tests, zero failures |

Exact tested executable hashes are in VALIDATION.json. Native tests used private
original-data fixtures; these were neither copied into the source snapshot nor
bundled with the archives. Screenshots in docs/images are selected actual
in-engine captures, with their separate provenance inventory.

Only build/tooling/docs/export changes were made for publication. Earlier
extensive 0.7.13 rendering/recording validation remains separate from these new
checks. The GitHub Actions workflow has been prepared but has not yet run on
GitHub. Technical checks are not a legal clearance of the P01/P02 audit findings.


---

<a id="src-third-party-readme"></a>

## Vendored PNG libraries

Vendored upstream stb PNG reader/writer, downloaded 2026-10-18.
Original headers are unmodified; dual license is in each header and THIRD_PARTY_NOTICES.txt.

stb_image.h SHA256 594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
stb_image_write.h SHA256 cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05

