# Source and release layout

The Git repository contains development source and documentation. Downloadable
assets are kept under GitHub Releases, separate from tracked source files:

- `OpenMR2-0.7.15-dev-source.tar.gz`: complete source snapshot, upstream source
  and license texts, tools, build scripts, documentation and graphics provenance.
- `OpenMR2-0.7.15-dev-Windows-x64.zip`: Windows executable and runtime documents.
- `OpenMR2-0.7.15-dev-Linux-x86_64.zip`: native Linux executable and runtime
  documents; uses host libraries, without bundled AppImage dependencies.
- `SHA256SUMS.txt`: SHA-256 hashes of the three downloadable archives.

These ZIPs do not contain original game data or FFmpeg. Users provide their
own GOG installation and optionally install an FFmpeg build supporting libx264
and AAC. Linux runtime packages are documented in the README.

The source tarball has one versioned top-level directory. Binary ZIPs have
separate platform-specific top-level directories and include LICENSE,
LICENSING.md, THIRD_PARTY_NOTICES.txt, license-status/audit documents and a
SOURCE.txt identifying the exact accompanying source archive and its hash.

## Maintainer workflow

1. Clone the existing repository into a separate directory.
2. From the working source tree, run `python tools/prepare_github.py --export
   PATH_TO_CLEAN_CLONE`. This preserves existing history and LICENSE and never
   stages, commits or uploads files. It refuses a destination with local changes.
3. Review `publication-manifest.json` and PUBLICATION_AUDIT.md. After intentional
   source edits, refresh the inventory with `python tools/prepare_github.py
   --refresh .` in the export repository and review the resulting diff.
4. Run `python tools/prepare_github.py --check .`, both platform builds and
   appropriate local tests. CI compiles without original game fixtures.
5. Create the tarball/ZIPs with `tools/package_github_release.py`, passing the
   explicit source repository, tested Windows/Linux executables and output path.
6. Push the reviewed commit and attach the separate archives to the matching
   GitHub prerelease. A development version is a prerelease, not a stable release.

No automatic deployment or binary upload occurs in the build workflow. Archive
creation is a technical packaging action, not clearance of unresolved decoder
or artwork redistribution rights. See LICENSE_STATUS.md and PUBLICATION_AUDIT.md.
