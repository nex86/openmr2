# Deferred work

## Video export and replacement import (requested 2026-10-18)

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

## Original DOS renderer parity

Recover original shade-table selection, indexed shadow/transparency lookup,
scanline fill/clipping/rounding and polygon ordering. Compare matched camera,
vehicle, frame and palette states against DOSBox captures before claiming
pixel-exact rendering. See RETRO_RENDERER.md for confirmed evidence and limits.
