# Draft: OpenMR2 0.7.13-dev

0.7.13-dev (2026-10-19): global F1–F12 shortcuts, race-preserving mode-specific options, scrollable cinematic/model Extras, quality-98 JPEG screenshots and H.264/AAC MP4 recording with transient top-right notifications. Revived background filtering now includes original picture layers. Disabled arrows retain transparency. Retro keeps source palette colors instead of recoloring all layers through a video's changing palette. Shop notices use a small transparent overlay; the audio-off hint is beneath the sliders. Loading WAIT.16 music uses race volume even with menu music muted. The Retro-only surprise surprise! checkbox is the explicit opt-in effects-only exception; every other Retro lock is retained. See RECORDING.md.

Revived adds AI-redrawn HUD/symbol/cursor assets and eleven uniformly aligned shop tiles, stronger backward grinding sparks and short impact bursts. Embedded controls, main-menu artwork and portraits remain original. Experimental video upscaling offers FSR, HQ4x, 4xBR and Lanczos3 GPU filters; it is disabled by default and unavailable in Retro. Native Retro output uses the active original 256-color palette before optional output filtering. See VIDEO_UPSCALING.md, RETRO_RENDERER.md and assets/revived_ui/README.md for scope and limitations. Offline video export/import is deferred in TODO.md.

**Private development candidate. Not cleared for public release.**

This candidate restores the original track HUD icons and fine status bars, fixes race-engine loops persisting across game-over transitions, and introduces verified German/English/French GOG data recognition. Language-dependent video-decoder addresses are normalized so the tested original track frames decode consistently across all three data sets.


Only the German, English and French GOG editions are supported; original CD support is planned later. Bring your own game files. No original assets are included.

Before publication: resolve the source/decoder license, complete third-party redistribution compliance, finish localization and HUD typography/layout review, and perform interactive gameplay/audio/controller/clean-machine acceptance tests. Existing approximations in physics/AI/camera behavior remain. Linux controller support is absent; Linux fullscreen/audio need broader desktop testing. See TEST_REPORT.md and WORK_STATUS.md for exact evidence and outstanding items.

## 0.7.2 changes

One-time local/selected-folder verification now shows an English progress dialog and writes Files_OK plus the verified path/edition to the setup CFG. Cached starts check presence only; use Files_OK=false to recheck changed contents. Retro Mode replaces the previous name in all languages. Console output reports graphics/models/videos and inventory/settings events in English, omits sound filenames and periodic FPS, and stops recording/formatting diagnostics while disabled.

## 0.7.3 changes

Retro mode now locks modern graphics options while retaining the user's Revived settings across mode changes/restarts. Native race/replay resolution is 320x400. The Retro model path uses affine textures, integer pixel positioning and omits added modern lighting. Complete software-rasterizer parity remains open; see RETRO_RENDERER.md.


## 0.7.4 vehicle and audio corrections

All 342 vehicle texture files (six worlds, three editions) are decoded and checked pixel by pixel: reserved indices 248..255 use DATA640/PAL_RACE.HLZ, while all other colors remain unchanged. This fixes placeholder magenta/green replacing red/yellow accents for every vehicle texture set. The preview camera horizontal basis is corrected, and previews rotate 3.5 times faster.

Game Over now stops all mixer channels, clears stale engine instances and flushes queued output before the finite entry effect. Native PCM checks require silence after that effect. Windows additionally tests the actual transition through a mocked waveOutReset queue; no speaker-listening acceptance is claimed. All 14 cars are captured in both modes on each platform; see test-results.json for results.


## Custom textures (0.7.5)

Debug / Cheats > Dump textures exports original static assets and material references to texture_dumps. Advanced Graphics > Enable custom textures loads edited PNGs only from texture_replacements, keeping source-relative LOCAL/CD paths and palette-qualified SPR frame names. Replacements are suppressed in Retro while the Revived preference is retained. Higher-resolution UI is composed as ordered GPU layers; geometry, hit boxes and original UV normalization remain unchanged. English diagnostics describe exports, loads, alpha, fallback and cache reload. Read custom_textures.txt for complete instructions and limits. UBB video frames and procedural graphics are outside the static-image replacement feature.


## 0.7.6 palette and export action corrections

Only explicit accent marker colors at palette indices 248/249 are replaced; all other entries, including white barrel skulls, are preserved. Dump textures is an action, with no saved toggle or cancellation on repeated clicks. Export progresses in every view, displays the same bar as track loading, stops automatically and plays the Apply sound once. Existing dump_textures config keys are ignored. Corrected original dumps refresh only exact, unchanged legacy outputs; edited exports/replacement PNGs are preserved.

Texture export/replacement folders are stored inside revived_config: revived_config/texture_dumps and revived_config/texture_replacements.


0.7.7 adds a compact options layout without dark panels, five independent volume sliders, zero-volume loading/playback gates and scalable Revived text throughout the game. Retro retains original bitmap fonts. See CONFIGURATION.md for audio migration and custom-font precedence.


0.7.8 restores the original per-car engine profiles and pitch curve, the old Retro options layout with original-size type, a separate Advanced sound menu with pixel bars, and a Retro-only graphics/debug list. See ENGINE_AUDIO.md and CONFIGURATION.md.


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

## 0.7.9 changes

# Revived Effects - 0.7.9

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

## Damage thresholds

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

## Renderer limits

The original scenery remains prerendered video. Dynamic shadows can be received
by the reconstructed road surface and rendered models; unavailable scenery meshes
cannot receive arbitrary accurate dynamic shadows. This is a directional shadow
map with soft filtering, not ray tracing. Fire and smoke combine camera-facing
particles and shaders; debris is solid 3D geometry. This is not a fluid simulation.
Historic replay files do not contain full damage or brake histories; those
unrecorded states cannot be reconstructed accurately from old recordings.

## Related gameplay fixes

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


## 0.7.10-dev

Revived missiles now leave a short fading smoke trail and have a small animated exhaust flame with a stronger ignition pulse. The independent **Missile effects** switch is under **Advanced Graphics > Revived Effects**. The submenu entry now uses an arrow. Retro retains its original presentation.

## 0.7.11-dev

Revived Effects now has six 0-200% sliders and Reset (100% restores the current tuned values). Vehicle lamps use original marker pixels and correct front/rear body locations, avoiding paint and wheels. Headlights/braking and running rear trails are stronger; lamps glow in both vehicle selections. Barrels/mines have large blast cores, shock waves, smoke and 48 flying debris pieces. The visible mode/menu name is consistently Revived.
