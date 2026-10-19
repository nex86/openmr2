# Keyboard and mouse controls

## Global shortcuts

| Key | Action |
| --- | --- |
| F1 | Switch between Revived and Retro Mode |
| F2 | Toggle texture/background filtering in Revived |
| F3 | Switch Original / 60 FPS in Revived |
| F4 | Toggle custom textures in Revived |
| F5 | Cycle Retro output filters: Point, Linear, HQ2x, BRZ, Interlacing, CRT |
| F6 | Cycle Retro scaling: 1x, 2x, 3x, 4x |
| F7 | Toggle experimental video upscaling in Revived |
| F8 | Cycle experimental video filters: FSR, HQ4x, 4xBR, Lanczos3 |
| F9 | Mute/restore all six Revived effect groups, preserving their preferences |
| F10 | Open the current mode's full options; Apply/Esc returns to the paused scene |
| F11 | Start/stop H.264/AAC MP4 recording; external FFmpeg required |
| F12 | Save a quality-98 JPEG screenshot |
| Alt+Enter | Toggle fullscreen on Windows; Linux uses the fullscreen menu option |

Modern graphics shortcuts, including F9, remain unavailable in Retro Mode.
Capture notifications appear briefly at the top right and are excluded from
saved media. Files go to `revived_config/recordings` and
`revived_config/screenshots`.

## Driving and racing

| Key | Action |
| --- | --- |
| Up / Down | Accelerate / brake; reverse follows the original drive behavior |
| Left / Right | Steer |
| Space | Fire selected front weapon |
| Enter | Use selected rear weapon |
| Tab | Switch front weapon slot |
| / | Switch rear weapon slot |
| Esc or P | Pause/resume, outside the race flyby |
| Left / Right, then Enter | Choose an action in the pause menu |

## Menus, shops and saved games

| Context / input | Action |
| --- | --- |
| Mouse movement / left click | Point to and activate controls |
| Arrow keys | Navigate menu entries; Left/Right adjust selected values/sliders |
| Enter; Space where offered | Activate selected item |
| Esc | Back/cancel; closes the full F10 options back to its original scene |
| Car shop: Left / Right | Previous / next car |
| Car shop: Enter | Select/buy car and continue to equipment |
| Equipment shop: Left / Right | Previous / next item |
| Equipment shop: Enter | Buy/sell selected item |
| Equipment shop: Up / Down | Increase / decrease repair amount |
| Equipment shop: R | Apply the selected repair |
| Save/load: Up / Down | Select save slot |
| Save/load: L or Enter / S | Load / save selected slot |
| Replay list: Up / Down | Select replay slot |
| Replay list: Enter / D | Play / delete selected replay |
| Replay playback: Space | Pause/resume |
| Replay playback: Left / Right | Seek backward / forward five seconds |
| Replay playback: Esc or Enter | Leave playback |
| Cinematic: Space, Enter or left click | Skip current clip |
| Cinematic: Esc | Skip remaining queued clips and continue |
| Name entry: letters, Space, Backspace | Edit name; Enter accepts, Esc leaves |
| Extras: Up / Down, Page Up / Page Down, wheel | Navigate/scroll cinematic list |

## Model viewer

Open **Debug / Cheats > Extras > Model viewer**. These keys are contextual;
they do not replace the global F1-F12 shortcuts.

| Input | Action |
| --- | --- |
| Left / Right | Previous / next model |
| Up / Down or Page Up / Page Down | Previous / next model animation step |
| Left mouse drag | Rotate view |
| Wheel or + / - | Zoom |
| Space | Toggle automatic rotation |
| R | Toggle the viewer's additional spin animation |
| L | Cycle available levels of detail |
| W | Toggle wireframe |
| B / G | Toggle background / ground |
| H | Toggle help |
| Home | Reset view and reload the selected model |
| Esc | Return to the game |

The developer-only standalone video mode (`--mode fmv`) additionally supports
Left/Right for previous/next video, Enter to restart, Space to pause/resume,
M to mute/unmute and H for help. Extras cinematics instead use the normal clip
controls above. Old global viewer/debug mode-jump keys are removed.
