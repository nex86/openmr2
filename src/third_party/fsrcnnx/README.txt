FSRCNNX x2 convolutional neural-network video upscalers

Unmodified upstream GLSL model sources from FSRCNN-TensorFlow release 1.1:
https://github.com/igv/FSRCNN-TensorFlow/releases/tag/1.1

Copyright (C) 2017-2021 igv. GNU Lesser General Public License version 3.0
or, at your option, any later version. Complete licenses are supplied in
COPYING.LGPL-3 and COPYING.GPL-3. Keep the notices in the shader sources.
provenance.json records release URLs, sizes and SHA-256 hashes.

The original model definitions and learned coefficients are included here;
OpenMR2 does not train or download a model at runtime. These are luma-only x2
networks. The 8-channel model has 14 passes and the 16-channel model 26 passes.
The final pass rearranges four predicted channels into a 2x2 output pixel tile.
Color/chroma reconstruction and optional sharpening belong to the port's
separate display pipeline, not to these upstream models.

OpenMR2's tools/build_video_cnn.py adapts the mpv HOOK/BIND/SAVE interface to
OpenGL 3.3 fragment passes with explicit texture bindings and clamped texel
fetches. It preserves learned coefficients, matrix ordering, activation
functions and the final subpixel-channel ordering. Input RGB is converted
to BT.601 luma in the input sampling adapter. Every intermediate tensor must
use a signed floating-point render target (RGBA16F); normalized color textures
cannot represent the model's negative features and residuals.

Corresponding adapter source: tools/build_video_cnn.py.
Generated adapter shaders: src/video_cnn_data.h.
Regenerate offline: python tools/build_video_cnn.py
Verify regeneration and structural checks:
  python tools/build_video_cnn.py --check --self-test

The renderer exposes these filters only for Revived video playback and track
backgrounds. Original video data and the Retro framebuffer are preserved.
