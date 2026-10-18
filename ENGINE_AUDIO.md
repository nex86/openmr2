# Original engine audio

The runtime reads car assignments, original profile levels and the complete
1,400-entry speed/playback-frequency curve from the user's verified M.EXE.
It does not synthesize fourteen different sounds or select a sample by vehicle
appearance. Original game audio files are required and are not bundled.

| Car | Original profile | Original sample | Original level |
|---|---:|---|---:|
| Thunder V. | 0 | SFX/TURBINE.WAV | 20 |
| Vipper | 1 | SFX/M_2.WAV | 20 |
| Astra B. | 2 | SFX/MOT3.WAV | 18 |
| Strata VII | 3 | SFX/M_2.WAV | 18 |
| CoCoon | 1 | SFX/M_2.WAV | 20 |
| BenHur 2000 | 2 | SFX/MOT3.WAV | 18 |
| MyWay | 3 | SFX/M_2.WAV | 18 |
| Traction XII | 1 | SFX/M_2.WAV | 20 |
| Futura IV | 2 | SFX/MOT3.WAV | 18 |
| Condor | 3 | SFX/M_2.WAV | 18 |
| Flyzz II | 1 | SFX/M_2.WAV | 20 |
| TonnCar | 2 | SFX/MOT3.WAV | 18 |
| Flyzz | 3 | SFX/M_2.WAV | 18 |
| Blatt 96 | 1 | SFX/M_2.WAV | 20 |

There are four profiles and three distinct samples; profiles 1 and 3 use the
same sample at different levels. Shared profiles are original behavior.
The engine volume slider applies to all profiles independently of sound effects.
The player and the audible nearby opponent use their own car's assignment.
Changing the audible opponent retires its former loop. Race teardown also
retires lost/stale loop handles for all three sample types.

## Read-only evidence

Addresses below refer to the loaded, relocated LE image, not raw file offsets.

| GOG edition | Car/profile table | Playback-frequency table |
|---|---:|---:|
| English | 0xDCFAC | 0x858B6 |
| French | 0xDD63C | 0x85F42 |
| German | 0xDD64C | 0x85F5A |

The profile table contains fourteen 32-bit indices. Immediately following it
are four records of 36 bytes: a 32-byte sample filename and a 32-bit level.
The model-name sequence in the executable confirms the same fourteen-car order.
English instructions at 0x45514..0x4553A select the player's sample through this
table; 0x4561B..0x456A3 apply its level and speed/frequency lookup. The lookup
uses half the magnitude in reverse. The curve has original gear drops at raw
speeds 300 and 600 and saturates at 22,050 Hz. All original engine samples used
here are 8-bit mono PCM at 22,050 Hz. The former MOTEURA/_MOTEUR2 fallback
samples are not the samples selected by this original table.

Run `python tools/inspect_engine_audio.py` against the three GOG fixtures.
It checks all 42 assignments, twelve profile records, sample metadata/hashes
and equality of all 4,200 pitch entries, writing only metadata to
build/engine-audio-research.json. It never modifies original files.

Native tests also load each edition and start real mixer voices for every car,
check correct sample/bus ownership, gear drops/reverse/clamping, opponent
sample changes and cleanup. The surrounding spatial mixer and driving model
remain the port's implementation; this work establishes the original sample
assignment and playback curve rather than full DOS mixer/physics equivalence.
