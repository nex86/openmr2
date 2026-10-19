# Contributing

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

For bug reports include the program version, OS/GPU, Revived/Retro mode, relevant
settings and reproduction steps. Redact personal paths and account details.
