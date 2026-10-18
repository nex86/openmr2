# Publication build validation

Validation on 2026-10-19 used the separate exported source tree, without
changing the installed development build or original game data.

| Check | Result |
| --- | --- |
| Windows x64 clean-source compile | Passed; all import libraries generated from .def files |
| Windows native regressions | 16,324 assertions, zero failures |
| Linux x86_64 clean-source compile | Passed on Ubuntu 24.04 |
| Linux native regressions | 16,314 assertions, zero failures |
| Actual Windows rendering | Six captures: race/options/car selection, both modes |
| Actual Linux rendering | Six captures: race/options/car selection, both modes |
| Export checker negative/positive cases | Eight tests, zero failures |

Exact tested executable hashes are in VALIDATION.json. Native tests used private
original-data fixtures; these were neither copied into the source snapshot nor
bundled with the archives. Screenshots in docs/images are selected actual
in-engine captures, with their separate provenance inventory.

Only build/tooling/docs/export changes were made for publication. Earlier
extensive 0.7.13 rendering/recording validation remains separate from these new
checks. The GitHub Actions workflow has been prepared but has not yet run on
GitHub. Technical checks are not a legal clearance of the P01/P02 audit findings.
