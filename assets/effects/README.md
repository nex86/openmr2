# Vehicle fire asset

Created for OpenMR2 using the built-in Imagegen tool (transparent output).
`vehicle-fire-source-v1.png` preserves the full generated RGBA image.
`vehicle-fire-atlas-v1.png` is the 512 x 512 runtime atlas; its four cells
are blended and distorted by the flame-flow shader in `src/g_effects.inc`.
`tools/build_effect_asset.py` embeds the PNG in `src/fire_atlas_data.h`.
The alpha channel is preserved; no black color key is used.

The renderer uses softer, smaller flames from 90% damage, growing at each percentage point,
and broader flames on a wreck. All graphics are project assets, not extracted original game art.

## Final generation prompt

Use case: photorealistic-natural. Asset type: RGBA flame texture atlas for a real-time vehicle fire particle shader. Generate one square sprite atlas, a precise 2 by 2 grid of four distinct natural flame clusters, all isolated on genuinely transparent background. Each cell contains a full upright irregular cluster of realistic burning gasoline/vehicle fire, rooted at the bottom of its cell with curling orange flame tongues, hot pale yellow cores, dark orange wisps, intricate semi-transparent feathered edges. Keep a transparent gutter around every cell so nothing touches another cell or the atlas edge. Photographic fire texture, natural turbulent tongues and ragged silhouettes, no circular fireball, no solid disc, no halo ring, no neon outline, no smoke (smoke is rendered separately), no objects, no car, no lettering or grid lines. Flame roots low, tips upward; transparent gaps between individual tongues. These are four variations of one fire asset, not a scene.
