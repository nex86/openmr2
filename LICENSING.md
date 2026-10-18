# License scope

OpenMR2's independently created project code is licensed under the GNU General
Public License version 3 only (GPL-3.0-only), to the extent the contributors hold
the applicable rights. The repository's LICENSE contains the full license.
Contributions to project code use the same terms; contributors must have the
rights to submit them.

| Component | Terms / status | Notices and provenance |
| --- | --- | --- |
| Independently created project code and tools | GPL-3.0-only, within contributor-held rights | LICENSE and owner statement in LICENSE_STATUS.md |
| `src/ubbdec.inc` | Direct original-executable translation; redistribution rights unresolved | Host comment in `src/media.cpp`, address labels and CPU operations |
| `assets/revived_ui/` and `src/remastered_ui_data.h` | AI edits of original artwork; no blanket GPL grant for original artwork | Asset README, prompts, catalog and generated source sheets |
| Original game executable, disc images, graphics, videos and audio | Proprietary; externally supplied by the user | Absent from source export |
| Project fire atlas and `src/fire_atlas_data.h` | GPL-3.0-only to the extent contributors hold rights | Text-only generation prompt in `assets/effects/README.md` |
| Rajdhani font, SDF atlas and embedded font header | SIL OFL 1.1 | `assets/fonts/OFL.txt`, source URL/hash |
| stb image headers | MIT / public-domain dual license | Header notices and THIRD_PARTY_NOTICES.txt |
| AMD FSR1 | MIT | `src/third_party/fsr1/LICENSE.txt` and provenance.json |
| HQ2x / HQ4x shaders and lookup tables | LGPL-2.1-or-later | `src/third_party/output_filters/` and `video_filters/` |
| xBR-lv2 shader | MIT | Upstream header in `src/third_party/video_filters/xbr-lv2.glsl` |
| xBRZ freescale shader | Contains MIT and GPLv3 portions | Full upstream header and XBRZ-COPYING.txt |
| Generated filter headers | Include the above third-party material under its original terms | Builders and upstream sources included |
| FFmpeg/libx264 | Optional external executable; exact build determines terms | Not bundled in source export; https://ffmpeg.org/legal.html |

Retain all upstream notices and license texts. Embedding an image, font or
shader in a C++ header does not erase its license or provenance.

A GPL file cannot grant rights that belong to the original game's rightsholder.
The decoder and derivative artwork findings remain unresolved. This tree must
not be described as a uniformly cleared GPL source distribution. See
PUBLICATION_AUDIT.md. No affiliation with or endorsement by the original game's
rightsholders is asserted.
