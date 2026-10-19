# Original physics: evidence, validation and remaining differences

Development revision: 0.7.14-dev. Both presentation modes use the same driving
kernel and the original complete-update cadence. The former subdivided Revived
physics option is removed; old configuration values cannot select it.

## Compared with the original executable

The tests run routines from a locally owned German GOG `M.EXE` in Unicorn and
compare numeric results with independently expressed C++ rules. They do not
embed, export or distribute executable instructions or game files.

Reference executable SHA-256:
`3d9c967a00c6dd15814adff51f09d830608c5c5c11cfdfe45a075e6fc64524fa`.
Runtime addresses below include the LE code base, unlike offsets in older notes.

| Routine | Runtime address | Cases | Compared outputs | Differences |
| --- | --- | ---: | ---: | ---: |
| Driving integration | 0x33d95 | 4,000 | 19 per case | 0 |
| Local straight-road wall response | 0x36d99 | 6,000 | 20 per case | 0 |
| Pair normal-velocity exchange | 0x37c4f | 6,000 | 4 per case | 0 |

The drive cases cover all three original profiles, player/opponent differences,
acceleration, braking, coasting, steering, variable update intervals, forward and
reverse motion, oil and grip states. Integer rounding, signed division, speed
limits, acceleration filtering and slip-dependent steering use the original
units: 51,200 time units/second and 1,024 angle units/turn. The original cosine
file is loaded from `DATA640/TABLEAU.COS`; its quadrature table has negative-sine
orientation. The fallback generates the same rounded values.

Wall cases cover both sides, all angle classes, player/opponent differences,
forward/reverse motion and varying incoming velocities. The original lateral
reflection is -75/256, forward retention 0xf200/65536. Motor speed, yaw response,
grip state and heading correction are separately retained; the port's previous
0.35/0.85 bounce and direct 0.45 heading kick were guesses and are removed.
Heading-dependent collision extents replace visual mesh width. The discrete
wall rules and hull extents are reproducible numeric measurements in
`original_wall_rules.h`, not an embedded executable or instruction translation.

The original road-curvature factors for all six tracks are 256/256. The invented
0.55 road assistance has been removed. Heavy-barrel contacts use the verified
normal-velocity exchange and measured relative-speed retention rather than
random lateral throws. Vehicle contacts no longer apply an invented instant
heading kick or derive engine speed by dividing by a near-zero heading cosine.

## Scope: not a claim of complete equivalence

These are isolated routine comparisons. They do **not** prove an entire race
matches DOS bit for bit. The road-position adapter still uses floating-point
segment interpolation and curvature; exact PST frame transitions, forks and
integer position accumulation need matched-state trajectory comparisons.

Vehicle and object contact detection, overlap resolution and contact-arm angular
responses are not yet fully equivalent to the original complete contact
dispatcher. The checked pair exchange is a component, not a certification of
the full car-versus-car response. Cone tumbling, mine/explosive-barrel impulses,
destroyed-car motion, damage accounting, opponent decision logic and recovery
also retain reconstructed approximations. Modern optional opponent rescue is
available only in Revived. Changing these remaining rules without executable
evidence would substitute new guesses for old ones.

Exact 100% original behavior is therefore **not claimed**. Further work needs
original-executable reference tests for those dispatchers, complete track-frame
integration and reproducible same-input full-race traces. Renderer differences
are listed separately in `RETRO_RENDERER.md`.

## Reproducing the reference checks

Install development-only Python packages `unicorn` and `capstone`. Compile
`tests/original_drive_cli.cpp`, `tests/original_wall_cli.cpp` and
`tests/original_contact_cli.cpp` with a C++17 compiler. Then run each corresponding
`tests/original_*_reference.py` with `--game` pointing at the German GOG fixture,
`--cli` pointing at its compiled adapter and `--output` pointing at a JSON report.
The drive check uses `--cases 4000`; the others default to 6000. On Windows,
`--wsl` can execute a Linux CLI in the local Ubuntu-24.04 development distribution.
Game runtime builds require none of these Python packages.

`original_wall_reference.py --generate src/original_wall_rules.h` regenerates
the discrete numeric wall measurements. Keep original files and private
disassembly outside publication exports. Normal Windows/Linux regression tests
also check that the same multi-step drive sequence produces identical states in
Retro and Revived and that legacy settings cannot unlock modern Retro effects.
