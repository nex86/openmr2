# Revived Effects - 0.7.11

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

## Missile exhaust

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

Sustained grinding at the road boundary produces a dense, speed-dependent
backward shower of hot metallic sparks. A short wall impact produces a separate
brief burst, without enabling the continuous grinding emitter. Vehicle contact
sparks originate from the collision contact point and depend on relative motion;
stationary overlapping cars do not emit an endless shower. This also applies to
opponent-to-opponent contact. Spark bursts are rate-limited; individual particles
expire within 0.56 seconds. Disabling damage effects or entering Retro suppresses
these particles. No collision impulses or damage calculations are changed.

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

## Assets and validation

The generated fire source, RGBA runtime atlas and complete Imagegen prompt are
in `assets/effects/README.md`. `tools/build_effect_asset.py` regenerates the
embedded header without network access. No original game texture is replaced
on disk. The atlas is embedded in the executable and requires no separate install.

`tests/gameplay_graphics.inc` checks settings, scaling, original missile models
across all six worlds and three languages, sweeps, detonations, scrape audio,
all eight vehicle deaths, particle lifetimes and RNG isolation. The companion
`tests/gameplay_graphics.py` captures actual OpenGL output for effects, threshold
states, opponent effects, trails, Retro isolation and expiration.
