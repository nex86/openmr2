# Building 0.7.13-dev

The authoritative code is `src/`. The flat `.cpp/.inc/.h` files outside `src/` in the inherited working folder are obsolete copies and are excluded from source packages. Keep `g_menu_head.inc`, `g_menu_mid.inc` and `g_menu_tail.inc` authoritative: build scripts concatenate them into `g_menu.inc`.

Python tooling requires Python 3.11 or newer (tested: 3.14 on Windows and 3.12 on Ubuntu). Optional visual inspection/comparison tools use Pillow (tested: 12.1.1). Neither Python nor Pillow is needed to run the compiled game.

## Windows x64

Tested toolchain: LLVM Clang/LLD 23.1.3, PowerShell, Windows x64. A Windows SDK, Visual Studio and the MSVC C runtime are not required by the freestanding build.

```powershell
./build_windows.ps1
./tests/run_windows.ps1
```


Windows build output is `build/OpenMR2.exe`. Use `-LLVM` to override the LLVM bin directory. No original game data is required to compile.

## Linux x86_64

Tested environment: Ubuntu 24.04 under WSL, GCC 13, X11 headers, Mesa/llvmpipe for graphics tests. Install `g++ libx11-dev libgl1 libgl1-mesa-dri libpulse0`; `xvfb` is useful for noninteractive graphics tests. System package versions and bundled library hashes are recorded during packaging.

```sh
sh build_linux.sh
sh tests/run_linux.sh
python3 tools/fetch_appimage_tools.py
python3 tools/build_appimage.py
```

`build_linux.sh` accepts `CXX`. AppImage tools are downloaded from the official AppImage repositories; asset URLs and SHA-256 hashes are recorded in `build/tools/downloads.json`. The download script checks the upstream GitHub digest when supplied. AppImage output: `build/OpenMR2-0.7.13-dev-x86_64.AppImage`.

The AppImage build bundles transitive non-glibc dependencies, OpenGL dispatch/client libraries and PulseAudio client libraries. GPU-specific drivers remain system components. Its baseline is glibc 2.39; older distributions have not been verified. Public binary redistribution must wait for the license/corresponding-source audit described in `LICENSE_STATUS.md`.

## Original-data tests

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
