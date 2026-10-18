"""Read-only inspection of supplied GOG installations; never modifies game data."""
import collections
import hashlib
import json
import pathlib
import struct
import sys


def unpack(data, start=772):
    pos, bits, nb = start, 0, 0
    out = bytearray()

    def bit():
        nonlocal pos, bits, nb
        if not nb:
            bits = struct.unpack_from('<I', data, pos)[0]
            pos += 4
            nb = 32
        nb -= 1
        return (bits >> nb) & 1

    while True:
        if bit():
            out.append(data[pos])
            pos += 1
            continue
        if bit():
            w = struct.unpack_from('<H', data, pos)[0]
            pos += 2
            off, count = (w - 65536) >> 3, w & 7
            if not count:
                count = data[pos]
                pos += 1
                if not count:
                    break
        else:
            count = (bit() << 1) | bit()
            off = data[pos] - 256
            pos += 1
        for _ in range(count + 2):
            out.append(out[len(out) + off])
    return bytes(out)


def picture(path):
    d = path.read_bytes()
    w, h = struct.unpack_from('<HH', d, 768)
    return w, h, d[:768], unpack(d)


if __name__ == '__main__':
    from PIL import Image
    root = pathlib.Path(sys.argv[1]).resolve()
    dest = pathlib.Path(sys.argv[2]).resolve()
    dest.mkdir(parents=True, exist_ok=True)
    report = {}
    for language in ('English', 'French', 'German'):
        game = root / ('Megarace 2 ' + language)
        entries = {}
        for name in ('M.EXE', 'TEXTE/GAME.CFG', 'TEXTE/MENU.USA', 'DATA640/ARMES.HLZ', 'DATA640/ICON.SPR', 'DATA640/MENU0.HLZ'):
            p = game / name
            if p.exists():
                d = p.read_bytes()
                entries[name] = {'size': len(d), 'sha256': hashlib.sha256(d).hexdigest()}
                if p.suffix == '.HLZ':
                    w, h, pal, px = picture(p)
                    entries[name].update(width=w, height=h, decoded=len(px), colors=collections.Counter(px).most_common(12))
                    if len(px) == w * h:
                        im = Image.frombytes('P', (w, h), px)
                        im.putpalette(pal)
                        im.convert('RGB').resize((w * 3, h * 3), Image.Resampling.NEAREST).save(dest / (language + '-' + p.stem + '.png'))
        cfg = (game / 'TEXTE/GAME.CFG').read_bytes()
        entries['game_cfg_hex'] = cfg.hex()
        entries['text_files'] = sorted(p.name for p in (game / 'TEXTE').iterdir())
        report[language] = entries
    (dest / 'assets.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
