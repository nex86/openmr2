"""Compare real graphics with deterministic virtual timing; requires original German data."""
import hashlib
import argparse
import json
import os
import pathlib
import shutil
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--seconds', type=int, choices=range(10, 31), default=10)
seconds = parser.parse_args().seconds
platform = 'windows' if os.name == 'nt' else 'linux'
game = root.parents[1] / 'Megarace 2 German'
traces = []
runs = []
for mode, fps in [('original', 25), ('60fps', 60)]:
    run = root / 'build' / ('pacing-' + platform) / mode
    run.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    if platform == 'windows':
        exe = run / 'OpenMR2.exe'
        shutil.copy2(root / 'build/OpenMR2.exe', exe)
        config = run / 'revived_config'
        command = [str(exe)]
    else:
        config = run / 'data/OpenMR2/revived_config'
        command = ['xvfb-run', '-a', str(root / 'build/OpenMR2-0.7.11-dev-x86_64.AppImage')]
        env.update(APPIMAGE_EXTRACT_AND_RUN='1', LIBGL_ALWAYS_SOFTWARE='1', XDG_DATA_HOME=str(run / 'data'))
    config.mkdir(parents=True, exist_ok=True)
    (config / 'openmr2_setup.cfg').write_text(
        f'debug_console = 1\nframe_rate = {mode}\nsound_effects = 0\nrace_music = 0\nmenu_music = 0\nlance_boyle_videos = 0\n')
    trace = config / 'openmr2_trace.csv'
    shot = run / 'race.bmp'
    trace.unlink(missing_ok=True)
    shot.unlink(missing_ok=True)
    command += ['--game', str(game), '--size', '640x480', '--track', '1', '--frame', '150',
                '--trace', '--demo', str(seconds), '--shotfps', str(fps), '--shot-after', str(fps * seconds), '--shot', str(shot)]
    kwargs = {}
    if platform == 'windows':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        kwargs['startupinfo'] = startup
    with (run / 'run.log').open('w') as log:
        proc = subprocess.run(command, cwd=run, env=env, stdout=log, stderr=subprocess.STDOUT,
                              timeout=180, **kwargs)
    if proc.returncode or not shot.exists() or not trace.exists():
        raise RuntimeError(f'{platform} {mode}: graphics/trace run failed')
    rows = trace.read_text().splitlines()
    if len(rows) < 1 + 8 * 200:
        raise RuntimeError('Insufficient trace coverage')
    traces.append(rows)
    runs.append({'mode': mode, 'virtual_render_hz': fps, 'minimum_requested_frames': fps * seconds,
                 'trace_rows': len(rows) - 1, 'last_update': int(rows[-1].split(';')[0])})
    print(platform, mode, 'virtual render Hz:', fps, 'trace updates:', runs[-1]['last_update'], flush=True)
common = min(map(len, traces))
if not all(rows[:common] == traces[0][:common] for rows in traces):
    raise RuntimeError('Rendered modes produced different canonical state traces')
result = {'platform': platform, 'pass': True, 'fixture': 'German Foundry', 'minimum_virtual_seconds': seconds,
          'common_trace_rows': common - 1, 'common_last_update': int(traces[0][common - 1].split(';')[0]),
          'trace_sha256': hashlib.sha256(('\n'.join(traces[0][:common]) + '\n').encode()).hexdigest(), 'runs': runs}
(root / 'build' / (platform + '-render-pacing-results.json')).write_text(json.dumps(result, indent=2) + '\n')
print('PASS: all common per-update vehicle/RNG/camera trace rows identical', flush=True)
