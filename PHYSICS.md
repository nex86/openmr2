# Driving and AI status - 0.7.15-dev

## Active implementation

0.7.15 restores the driving, steering/grip integration, road assistance,
heading-dependent movement, wall reaction, car contacts and heavy-object
response used in 0.7.13. This rollback applies to both presentation modes.
The rejected 0.7.14 fixed-point integration is no longer included by the game.
The removed subdivided-physics checkbox stays removed.

The isolated original-executable routines in original_drive.h,
original_wall.h and original_contact.h remain research/test material only.
They must not be described as the active driving implementation.

## Regression and evidence

The original-routine comparisons in 0.7.14 passed, but did not validate the
complete feedback loop formed by the existing AI controller, road-frame
adapter and new integration. Full rolling-start tests now reproduce the user's
reported regression: the rejected integration increased RMS heading variation
by roughly eight to ten times across the six tracks. It also caused incidental
starting-grid contact damage on Tibet and Bayou in the fixed test sequence.

The 0.7.15 real rolling-start probe uses all eight cars on all six original
tracks for 75 updates (three seconds) each, producing 3,600 vehicle samples.
Autopilot state, road node, lateral position, heading, steering state, lateral
velocity, speed and damage agree with the accepted 0.7.13 reference at every
sample. There is no starting-grid damage in this test. Results are stored in
`docs/physics/rolling-start-0715.json`.

Impact sounds now select among the three original CHOC1/CHOC3/CHOC5 samples,
exclude immediate repeats and use only the separate audio random generator.
Driving/AI randomness is unaffected. Existing contact cooldowns from 0.7.13
are restored; continuous wall grinding retains its separate FROTTE2 loop.

## Reproducing the rolling-start comparison

Compile tests/rolling_start_probe.cpp with C++17 alongside src/media.cpp on
Linux. Run it from the normal native regression build directory, with the same
privately owned German GOG fixture arrangement as tests/run_linux.sh. Redirect
stdout to a CSV. The probe uses the real race_update, original grid positions,
all six real track files, eight cars and fixed seeds, without rendering.

Run tests/rolling_start_compare.py with --reference pointing at a stable 0.7.13
probe CSV, --candidate at the new CSV and --output at a JSON report. It checks
all 3,600 rows and eight driving state fields with a 0.000001 tolerance.
Original game files and private recordings are not included in the repository.

## Original parity remains incomplete

The restored implementation is an accepted gameplay approximation, not proof
of exact DOS physics. Its steering response, road interpolation, collision
impulses, opponent decisions and object behavior still contain reconstructed
rules. The isolated research kernels remain useful references, but replacing
the active model again requires controller/adapter integration and complete
trajectory testing, including rolling starts. Passing an isolated routine test
is insufficient evidence for a replacement driving model.

No claim of 100% original physics or renderer equivalence is made. Renderer
limits and strict Retro separation are documented in RETRO_RENDERER.md.
