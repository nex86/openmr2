"""Copy lossless real-game test captures into the README gallery, without retouching."""
from pathlib import Path
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
images = root/'docs/images'
images.mkdir(parents=True, exist_ok=True)
selected = {
    'retro-race.png': 'remastered-ui-windows/retro-hud-original.png',
    'revived-race.png': 'remastered-ui-windows/track1-ai.png',
    'retro-options.png': 'revision-0713-windows/retro-ui-options.png',
    'revived-options.png': 'revision-0713-windows/ui-options.png',
    'retro-car.png': 'preview-0716/car-1-mode-1.png',
    'revived-car.png': 'preview-0716/car-1-mode-0.png',
    'effect-explosion.png': 'light-effects-windows/barrel-350.png',
    'effect-sparks.png': 'gameplay-graphics-windows/contact-sparks.png',
    'effect-headlights.png': 'light-effects-windows/car13-headlights.png',
    'effect-brakelights.png': 'light-effects-windows/car00-brake.png',
    'effect-damage.png': 'gameplay-graphics-windows/damage-100.png',
    'effect-oil.png': 'gameplay-graphics-windows/revived-oil.png',
}
records = []
for name, source in selected.items():
    path = root/'build'/source
    data = path.read_bytes()
    assert data.startswith(b'\x89PNG\r\n\x1a\n'), source
    shutil.copyfile(path, images/name)
    records.append({'image': name, 'source_test_capture': source,
                    'sha256': hashlib.sha256(data).hexdigest()})
(images/'provenance.json').write_text(json.dumps({
    'version': '0.7.16-dev', 'type': 'actual OpenMR2 deterministic in-engine test captures',
    'retouching': 'none; copied byte-for-byte from lossless PNG captures',
    'note': 'Retro means OpenMR2 Retro renderer, not the original DOS executable. '
            'Mode viewport/scaling may differ. Assets shown retain their respective rights.',
    'images': records,
}, indent=2)+'\n', encoding='utf-8')
print(f'Copied {len(records)} lossless captures for README comparisons/effect gallery.')
