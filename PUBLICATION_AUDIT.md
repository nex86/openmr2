# GitHub publication audit

Date: 2026-10-19. Target: https://github.com/nex86/openmr2.

## Established provenance

The project owner confirms the port through 0.6.4 was created with Claude for
this project. The unidentified upstream-author question is resolved by that
statement. It does not establish that every line is independent of the original
executable. Existing GitHub history and its GPL-3.0 LICENSE are preserved in
the local preparation clone.

The source export is a complete development snapshot. Unresolved files remain
identified; technical completeness does not mean all rights have been cleared.

## P01: directly translated executable code

`src/media.cpp` explicitly calls `src/ubbdec.inc` a static translation of the
original self-modifying x86 decoder. It retains original instruction-address
labels, registers, flag calculations, self-modifying operand locations and
dispatches. This decoder is used for cinematics and racing backgrounds.

Resolution requires establishing applicable redistribution permission/legal
basis or replacing it with an independently authored decoder of documented
provenance. A generic emulator loading the user's original executable is
another architecture to assess, including performance; it is not automatic
legal clearance. Renaming or relocating translated code does not resolve P01.

## P02: artwork derived from original sprites

`assets/revived_ui/README.md` records edit-mode AI transformations of original
HUD/icons: 49 selected sprites and 59 source/frame/palette identities. Both
PNGs and `src/remastered_ui_data.h` contain the resulting artwork.

Resolution requires applicable permission/legal basis, independently designed
replacement artwork, or exclusion of both the PNGs and embedded representation
from a public build while retaining runtime fallback to user-supplied originals.
The fire atlas is separate: its documented input was a text-only prompt.

## P03: previous binary packages

The source export excludes existing Windows ZIPs, FFmpeg and AppImages. Those
private artifacts need a separate audit of exact bundled components, matching
corresponding sources, notices and LGPL obligations before being advertised as
compliant public binary releases. Host-installed FFmpeg avoids distributing
that encoder binary; it does not resolve P01/P02 in OpenMR2 itself.

## P04: remaining behavioral reconstruction

Race/menu/track code includes original addresses, constants, tables and
algorithms. Matching functionality and file formats does not by itself establish
copied protected expression. These references are review evidence, not proof
that every such file is infringing or independently implemented.

## Technical preparation

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

## External requirements

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
