# GPU video upscaling (Revived only)

Advanced graphics provides a Video upscaling (experimental) checkbox and a separate filter
selector: FSR, HQ4x, 4xBR and Lanczos3. These process only decoded cinematic and
track-video textures before camera reprojection/composition. They do not filter
the HUD, fonts, static menu backgrounds or vehicle textures. The setting is
unavailable and hidden in Retro; the selected Revived preference is retained.

`video_upscaling = 0/1` enables the feature. `video_filter = 0/1/2/3` selects
FSR/HQ4x/4xBR/Lanczos3. The feature defaults off; the initial filter is FSR.

- FSR uses AMD's original MIT-licensed FSR1 EASU spatial reconstruction followed
  by RCAS sharpening. At large scale ratios, two 2x EASU stages precede RCAS.
- HQ4x uses the upstream two-pass HQx implementation with its genuine 4x lookup
  table. Its pattern classification can make video imagery look more like
  smoothed pixel art. The shader sources are LGPL-2.1-or-later.
- 4xBR uses Hyllian's MIT-licensed xBR-lv2 shader at a 4x scale. It is a pixel-art
  edge smoother, not a neural restoration model.
- Lanczos3 uses two separable six-tap windowed-sinc passes. This offers a more
  conventional reconstruction of photographic footage; existing compression
  noise or source aliasing cannot be recovered into true new detail.

All processing runs on the existing OpenGL GPU. There is no inference runtime,
neural-model download, frame generation, temporal detail invention or change to
the source frame rate. Identical source frames yield identical filtered pixels.
This avoids a new temporal model's unstable invented detail; it cannot promise
that compression noise or aliasing in the original footage will never shimmer.

Four bounded per-texture caches retain independently decoded/interpolated video
sources, so a repeated source frame does not need another upscale. Reopening a
video changes its generation identifier and invalidates the old cached output.
FSR and Lanczos use 2x/4x working images with a 2048-pixel soft limit; device
texture limits are checked. Failed shader compilation/allocation falls back to
the original sampling path. Disabling the feature or entering Retro bypasses
preprocessing; no background inference or CPU upscaling continues.

FSR's GL3.3 shader adapter requests ARB_shading_language_packing and
ARB_gpu_shader5. If these extensions are unavailable, FSR falls back to the
original video sampling; the other filter choices remain independent.

There is no universally best filter for all of this game's video material.
FSR emphasizes reconstructed edges, Lanczos3 is a useful alternative for natural
footage, and HQ4x/xBR intentionally smooth pixel patterns. Performance depends
on GPU, source dimensions and selected filter. Synchronous timing is restricted
to the explicit developer `--graphics-test 16` capture path, not normal gameplay.

Upstream references and complete source/license texts:
[AMD FSR1](https://gpuopen.com/fidelityfx-superresolution/),
[HQx preset](https://github.com/libretro/glsl-shaders/blob/master/hqx/hq4x.glslp),
[xBR-lv2](https://github.com/libretro/glsl-shaders/blob/master/xbr/shaders/xbr-lv2.glsl).
The source folders `src/third_party/fsr1` and `src/third_party/video_filters`
contain provenance hashes. Rebuild adapters with `tools/build_video_fsr.py` and
`tools/build_video_filters.py`; no network access is needed for rebuilding.

Offline video export/replacement import is deferred in TODO.md. It needs exact
frame/audio timing and track-camera mapping; it is not part of this filter option.
