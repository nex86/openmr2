# Screenshots and recordings

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

## Encoder installation

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

# Global function keys

| Key | Action |
|---|---|
| F1 | Switch Revived / Retro Mode |
| F2 | Toggle Revived texture filtering, including backgrounds |
| F3 | Switch Original / 60fps in Revived |
| F4 | Toggle custom textures in Revived |
| F5 | Cycle the Retro output filter |
| F6 | Cycle Retro scaling 1x–4x |
| F7 | Toggle experimental video upscaling in Revived |
| F8 | Cycle FSR / HQ4x / 4xBR / Lanczos3 in Revived |
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

# Strict Retro mode

Modern effects are always disabled in Retro. The former effects Easter egg was
removed in 0.7.14; old `retro_surprise` settings are ignored and removed on save.
F9 mutes/restores only Revived effects and leaves their individual preferences intact.
