"""Compare the active pilot/drive/road/car-contact loop with executed DOS code.

Requires Unicorn, the locally owned German GOG installation, and a compiled
original_race_probe. Track fixtures are extracted only into a temporary folder.
Higher-level tactics, weapons, road objects and retirement are excluded.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import io
import json
import subprocess
import tempfile
from original_fixture import OriginalMachine, original_memory, wsl_path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--game', type=Path, required=True)
p.add_argument('--cli', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--steps', type=int, default=1200)
p.add_argument('--wsl', action='store_true')
a = p.parse_args()
game, cli = a.game.resolve(), a.cli.resolve()
memory, cosine = original_memory(game)
offsets = [0x1c, 0x3c, 0x44, 0xb8, 0x70, 0x6c, 0x80, 0x38]
fields = ['node', 'x', 'angle', 'steer', 'lateral', 'forward', 'speed', 'along']
failures = []; compared = 0
with tempfile.TemporaryDirectory(prefix='openmr2-physics-') as temporary:
    directory = Path(temporary)
    command = [str(cli), str(game), str(directory), str(a.steps)]
    if a.wsl:
        command = ['wsl', '-d', 'Ubuntu-24.04', '--', wsl_path(cli), wsl_path(game), wsl_path(directory), str(a.steps)]
    run = subprocess.run(command, text=True, capture_output=True, check=True)
    actual = list(csv.DictReader(io.StringIO(run.stdout)))
    assert len(actual) == 6 * a.steps * 8
    for track in range(6):
        reference = OriginalMachine(memory); m = reference.machine
        put, get, call = reference.put, reference.get, reference.call
        pst, base = 0x1e323c, 0xacf34
        raw = (directory / f'track-{track}.PST').read_bytes(); m.mem_write(pst, raw)
        for node in range(len(raw) // 128): put(pst + node*128 + 124, 0)
        globals_initial = {0xb36f0:0, 0xb3900:7, 0xb3904:0, 0xb3810:0,
                           0xb37d0:2048, 0xb37a0:256, 0xb39b0:-1, 0xd894f:0,
                           0x88038:0, 0xb38a4:0, 0xb38b8:0, 0xb39e4:400,
                           0xb39e8:-200, 0xb39ec:-500, 0xb39f0:128, 0xb39f4:-128}
        for address, value in globals_initial.items(): put(address, value)
        profiles = [3,1,2,1,2,3,1,2]
        for i in range(8):
            car = base + i*512; m.mem_write(car, bytes(512)); put(car+0xcc, 0xa8180)
            rank = 7 if i == 0 else i-1; m.mem_write(car+0x1ff, bytes([rank])); call(0x334e4, esi=car)
            for off, value in {0xc:1, 0x64:518, 0x6c:518, 0x80:518, 0xc0:518,
                               0x114:518, 0x118:8, 0x14c:65535, 0x1a0:-1,
                               0x1ac:1, 0x1b0:profiles[i]}.items(): put(car+off, value)
            node = get(car+0x1c); head = get(pst+node+124)
            put(car+0x24, head); put(car+0x20, 0)
            if head: put(head+0x20, car)
            put(pst+node+124, car)
        for step in range(a.steps):
            call(0x3484a)
            for i in range(8):
                put(base+i*512+0x14c, 65535)
                call(0x3b41d, ebp=base+i*512)
            call(0x33a0d)
            for i in range(8):
                car = base+i*512
                wanted = [get(car+off) for off in offsets]; wanted[0] //= 128
                row = actual[(track*a.steps+step)*8+i]
                differences = {name:{'port':int(row[name]),'original':value}
                               for name,value in zip(fields,wanted) if int(row[name]) != value}
                if differences and len(failures)<12:
                    failures.append({'track':track,'step':step,'car':i,'differences':differences})
                compared += bool(differences)
report = {'samples':len(actual), 'tracks':6, 'cars':8, 'seconds_per_track':a.steps*.04,
          'compared_fields':fields, 'failures':compared, 'first_failures':failures,
          'original_exe_sha256':hashlib.sha256((game/'M.EXE').read_bytes()).hexdigest(),
          'scope':'fixed pilot, drive, shared 2D road frames, car contacts, wall response; no tactics, weapons, road objects or retirement'}
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2));raise SystemExit(bool(compared))
