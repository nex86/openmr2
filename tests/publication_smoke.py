"""Render actual release executables in both modes using an explicit private fixture."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--game', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--receipt', type=Path, required=True)
args = parser.parse_args()
binary = args.binary.resolve()
game = args.game.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
receipt = '\n'.join(line for line in args.receipt.read_text().splitlines()
                    if line.startswith(('Files_OK =', 'verified_directory =', 'verified_language =')))
assert 'Files_OK = true' in receipt, 'Expected an existing private verification receipt'
original_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
if os.name == 'nt':
    shutil.copyfile(binary, output/'OpenMR2.exe')
    binary = output/'OpenMR2.exe'
cfg = output/'revived_config/openmr2_setup.cfg'
cfg.parent.mkdir(exist_ok=True)
captures = []
for mode in (0, 1):
    for screen in ('race', 'options', 'carshop'):
        name = ('retro' if mode else 'revived')+'-'+screen
        cfg.write_text(receipt+f'\nremember_startup_mode = 1\nstartup_mode = {mode}\n'
                       f'graphics_mode = {mode}\nretro_surprise = 0\nretro_scale = 2\n'
                       'integer_scale = 0\nretro_output_filter = point\nframe_rate = original\n'
                       'msaa = 0\nfxaa = 0\ntexture_filter = 0\n'
                       'sounds_volume = 0\nengines_volume = 0\nrace_music_volume = 0\n'
                       'menu_music_volume = 0\nmenu_sounds_volume = 0\n')
        shot = output/(name+'.bmp')
        cmd = [str(binary), '--game', str(game), '--size', '1280x960',
               '--shot', str(shot), '--shotfps', '25', '--car', '0']
        if screen == 'race':
            cmd += ['--track', '1', '--frame', '150', '--objtest']
        else:
            cmd += ['--screen', screen]
            if screen == 'carshop':
                cmd += ['--graphics-test', '15', '--effect-age-ms', '3142']
        env = dict(os.environ)
        kwargs = {}
        if os.name == 'nt':
            info = subprocess.STARTUPINFO()
            info.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            info.wShowWindow = 0
            kwargs['startupinfo'] = info
        else:
            cmd = ['xvfb-run', '-a'] + cmd
            env['LIBGL_ALWAYS_SOFTWARE'] = '1'
        subprocess.run(cmd, cwd=output, env=env, timeout=120, check=True, **kwargs)
        with Image.open(shot) as image:
            image = image.convert('RGB')
            assert image.size == (1280, 960)
            assert len(image.getcolors(2_000_000)) > 100, name+' is blank'
            image.save(output/(name+'.png'))
        captures.append({'name': name, 'pass': True})
        print('Rendered '+name, flush=True)
(output/'results.json').write_text(json.dumps({
    'binary_sha256': original_hash, 'captures': captures, 'failures': 0,
}, indent=2)+'\n')
