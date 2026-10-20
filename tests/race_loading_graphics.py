"""Verify counted race preparation through the actual Windows/Linux render loop.

Requires the supported private English fixture and a previously verified test
receipt. Partial screenshots inspect real unfinished work; completion runs use
ordinary wall-clock updates and never force the job counter or game state.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import time

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
PLATFORM = "windows" if os.name == "nt" else "linux"
RUN = ROOT / "build" / ("race-loading-" + PLATFORM)
EXE = ROOT / "build" / ("OpenMR2.exe" if os.name == "nt" else "openmr2_linux")
GAME = ROOT.parents[1] / "Megarace 2 English"
RECEIPT_KEYS = ("Files_OK =", "verified_directory =", "verified_language =")
CHECKS = []
REPORTS = {}


def check(condition, description):
    assert condition, description
    CHECKS.append({"check": description, "pass": True})


def verified_receipt():
    candidates = (
        ROOT / "build" / ("video-upscale-" + PLATFORM) / "revived_config/openmr2_setup.cfg",
        ROOT / "build" / ("light-effects-" + PLATFORM) / "revived_config/openmr2_setup.cfg",
    )
    for path in candidates:
        if not path.exists():
            continue
        lines = [line for line in path.read_text(encoding="utf-8").splitlines()
                 if line.startswith(RECEIPT_KEYS)]
        receipt = "\n".join(lines)
        values = {key.strip(): value.strip() for line in lines
                  for key, value in [line.split("=", 1)]}
        directory = values.get("verified_directory", "").replace("\\", "/").rstrip("/").casefold()
        expected = str(GAME).replace("\\", "/").rstrip("/").casefold()
        if (values.get("Files_OK", "").lower() in ("true", "1") and
                values.get("verified_language") == "0" and directory == expected):
            return receipt
    raise RuntimeError("Run an English fixture graphics test first to establish a verified test receipt")


def read_report(path):
    return {key.strip(): int(value.strip())
            for line in path.read_text(encoding="utf-8").splitlines()
            if "=" in line for key, value in [line.split("=", 1)]}


def config(mode, receipt):
    options = {
        "graphics_mode": mode,
        "startup_mode": mode,
        "remember_startup_mode": 1,
        "texture_filter": 1,
        "custom_textures": 1,
        "video_upscaling": 1,
        "video_filter": 5,
        "frame_rate": "original",
        "retro_scale": 1,
        "integer_scale": 0,
        "retro_output_filter": "point",
        "msaa": 0,
        "fxaa": 1,
        "effect_quality": 1,
        "effects_muted": 0,
        "real_time_shadows": 1,
        "vehicle_lights": 1,
        "damage_effects": 1,
        "modern_explosions": 1,
        "missile_effects": 1,
        "enhanced_oil": 1,
        "vehicle_reflections": 1,
        "lance_boyle_videos": 0,
        "sounds_volume": 0,
        "engines_volume": 0,
        "race_music_volume": 0,
        "menu_music_volume": 0,
        "menu_sounds_volume": 0,
    }
    return receipt + "\n" + "".join(f"{key} = {value}\n" for key, value in options.items())


def launch(mode, partial, receipt):
    name = ("retro" if mode else "revived") + ("-partial" if partial else "-complete")
    folder = RUN / name
    folder.mkdir(parents=True, exist_ok=True)
    executable = EXE
    if os.name == "nt":
        # Win32 changes cwd to the executable directory, so each run owns its
        # executable, config, reports and optional screenshot together.
        executable = folder / "OpenMR2.exe"
        shutil.copy2(EXE, executable)
    cfg = folder / "revived_config/openmr2_setup.cfg"
    cfg.parent.mkdir(parents=True, exist_ok=True)
    cfg.write_text(config(mode, receipt), encoding="utf-8")
    state_path = folder / "race-preload-state.txt"
    graphics_path = folder / "graphics-state.txt"
    for path in (state_path, graphics_path):
        path.unlink(missing_ok=True)
    command = [str(executable), "--game", str(GAME), "--size", "640x480",
               "--screen", "info", "--graphics-test", "20"]
    shot = folder / "loading.bmp"
    if partial:
        shot.unlink(missing_ok=True)
        command += ["--shot", str(shot), "--shot-after", "1", "--shotfps", "25"]
    else:
        command += ["--quit", "4"]
    environment = dict(os.environ)
    kwargs = {}
    if os.name == "nt":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        kwargs["startupinfo"] = startup
    else:
        command = ["xvfb-run", "-a"] + command
        environment["LIBGL_ALWAYS_SOFTWARE"] = "1"
    started = time.monotonic()
    process = subprocess.run(command, cwd=folder, env=environment, timeout=240,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, encoding="utf-8", errors="replace", **kwargs)
    (folder / "process.log").write_text(process.stdout, encoding="utf-8")
    check(process.returncode == 0, name + ": native application exits successfully")
    check(state_path.exists() and graphics_path.exists(), name + ": actual frame loop writes both reports")
    state, graphics = read_report(state_path), read_report(graphics_path)
    REPORTS[name] = {"state": state, "graphics": graphics,
                     "elapsed_seconds": round(time.monotonic() - started, 3)}
    check(not state["failed"] and graphics["gl_errors"] == 0,
          name + ": loading/race render loop has no resource failures or OpenGL errors")
    check(state["video_streaming"] == 1 and 0 < state["video_bytes"] < state["video_size"],
          name + ": video remains disk-streamed during preparation and racing")
    if partial:
        check(state["state"] == 4 and not state["complete"] and
              0 < state["completed_jobs"] < state["total_jobs"],
              name + ": screenshot captures genuine partial work before readiness")
        check(shot.exists(), name + ": partial loading screen is captured")
        image = Image.open(shot).convert("RGB")
        image.save(folder / "loading.png")
        filled = sum(r > 150 and g > 70 and b < 100
                     for r, g, b in [image.getpixel((x, 426)) for x in range(170, 470)])
        expected = int(300 * state["completed_jobs"] / state["total_jobs"])
        check(abs(filled - expected) <= 3,
              name + ": visible progress-bar fill matches completed resource jobs")
    else:
        check(state["state"] == 5 and state["complete"] == 1 and
              state["completed_jobs"] == state["total_jobs"],
              name + ": ordinary updates finish every job before entering the race")
        check(state["cars"] == 14 and state["objects"] == 13,
              name + ": every vehicle and race-object model is cached before racing")
        if mode:
            check(state["effects_shader"] == 0 and state["light_shader"] == 0,
                  name + ": Retro bypasses modern effect/light preparation despite saved preferences")
        else:
            check(state["effects_shader"] == 1 and state["light_shader"] == 1,
                  name + ": enabled effects and vehicle-light shaders are prepared before racing")
    print(name, "completed", state["completed_jobs"], "/", state["total_jobs"], flush=True)


RUN.mkdir(parents=True, exist_ok=True)
receipt = verified_receipt()
for mode in (0, 1):
    launch(mode, True, receipt)
    launch(mode, False, receipt)
destination = ROOT / "build" / f"{PLATFORM}-race-loading-results.json"
destination.write_text(json.dumps({"runs": 4, "captures": 2, "checks": CHECKS,
                                   "reports": REPORTS, "failures": 0}, indent=2) + "\n",
                       encoding="utf-8")
print(f"{PLATFORM}: 4 native runs, 2 loading captures, {len(CHECKS)} checks passed")
