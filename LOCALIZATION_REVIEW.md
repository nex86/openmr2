# Localization review - 0.7.6-dev

271 stable text keys were reviewed in German, English and French. Keys are separate from German display text, allowing natural phrasing without changing runtime identifiers. The original renderer uses Latin-1 with composed French accents. Translation lookup runs once before layout/drawing; player-entered leaderboard names use raw text rendering.

## Wording and terminology

- Quit menu: Exit / Reset; French Quitter / Réinitialiser. Abandoned-driver result text remains a separate context.
- Debug / Cheats, French Debug / Cheats, with consistent submenu headings.
- Antialiasing, Framerate, MSAA, FXAA, FPS and Integer Scaling are intentional technical labels. Regular help remains in the selected language.
- Game menu replaces Mark selection. Race results replaces Classification. Empty slot replaces the incorrect Do not save label; Load game and Current game describe the actual saved-game interface.
- Replay controls, model/video help, weapon descriptions and native Windows graphics errors are covered. Proper names and the original game's deliberate humorous fuel descriptions retain their identity.
- Composed track names in saved games/replays are translated before concatenation. User names and paths are preserved.

## Exceptions and checks

The initial missing-data introduction is always English by request. Until game data can be recognized, English is the fallback language. Filenames, OpenGL identifiers, machine-readable traces and technical log field names remain unchanged. The font-test sample intentionally shows German and French characters together. Original videos, voice, imagery and credits are read from the selected language's GOG data; no replacement media were invented.

Native tests resolve every catalog key in each language and check important labels against their actual menu widths using the original bitmap font. Runtime captures cover all three language sets. Additional Windows captures inspect shops, saves, replays, single-race setup, results-related screens and fonts. See test-results.json for actual completed runs.

This is an editorial/code/layout review, not a claim of a full human gameplay acceptance pass or native-speaker certification. Rare error paths and all possible dynamic text lengths still need broader acceptance.

Debug console output and on-screen debug metrics deliberately use English in every language. The console checkbox itself follows the UI language; filenames and upstream driver output remain unchanged.
