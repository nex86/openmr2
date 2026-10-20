"""Native-resolution light-trail visibility captures, including ordinary driving.

Baseline mode records measurements without accepting faint/no visible trails.
Verify mode requires an exposed red trail beyond the current lamp+bloom footprint.
Captures and configuration stay in an isolated build directory. Windows and
Linux (xvfb-run/Mesa) use the same image metrics and screenshot geometry.
"""
from pathlib import Path
import argparse
from collections import deque
import hashlib
import json
import os
import shutil
import subprocess

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
PLATFORM = "windows" if os.name == "nt" else "linux"


def pixel_data(image):
    method = getattr(image, "get_flattened_data", image.getdata)
    return method()


def mask_image(points, size):
    image = Image.new("L", size)
    image.putdata([255 if value else 0 for value in points])
    return image


def largest_component(points, width, height):
    remaining = {index for index, value in enumerate(points) if value}
    best = []
    while remaining:
        start = remaining.pop()
        queue, component = deque([start]), [start]
        while queue:
            index = queue.popleft()
            x, y = index % width, index // width
            for next_y in range(max(0, y - 1), min(height, y + 2)):
                for next_x in range(max(0, x - 1), min(width, x + 2)):
                    neighbor = next_y * width + next_x
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        queue.append(neighbor)
                        component.append(neighbor)
        if len(component) > len(best):
            best = component
    if not best:
        return 0, None
    return len(best), [min(index % width for index in best), min(index // width for index in best),
                       max(index % width for index in best) + 1, max(index // width for index in best) + 1]


def trail_metrics(dark, lamps, trails):
    """Separate the current lamp bloom from new, visibly red trail coverage.

    A two-level difference alone is almost invisible. Here new trail pixels must
    gain at least 18 red levels, remain visibly red in the actual image, and lie
    outside a two-native-pixel guard around the current lamp/bloom footprint.
    """
    width, height = dark.size
    footprint = []
    for pixel, source in zip(pixel_data(lamps), pixel_data(dark)):
        red, green, blue = pixel
        dr = red - source[0]
        footprint.append(dr >= 8 and red >= 45 and red > green * 1.35 and red > blue * 1.3)
    guard = mask_image(footprint, dark.size).filter(ImageFilter.MaxFilter(5))
    exposed, all_red = [], []
    gains = []
    for pixel, source, protected in zip(pixel_data(trails), pixel_data(lamps), pixel_data(guard)):
        red, green, blue = pixel
        dr, dg, db = (pixel[channel] - source[channel] for channel in range(3))
        visible = (dr >= 18 and red >= 90 and red > green * 1.6 and red > blue * 1.5
                   and dr >= dg * 1.2 and dr >= db * 1.2)
        all_red.append(visible)
        exposed.append(visible and not protected)
        if visible and not protected:
            gains.append(dr)
    count, bounds = largest_component(exposed, width, height)
    metrics = {"current_lamp_pixels": sum(footprint), "all_visible_red_trail_pixels": sum(all_red),
               "exposed_red_pixels": sum(exposed), "largest_exposed_component_pixels": count,
               "largest_exposed_component_bounds": bounds,
               "maximum_exposed_red_gain": max(gains, default=0),
               "median_exposed_red_gain": sorted(gains)[len(gains) // 2] if gains else 0}
    extent = max(bounds[2] - bounds[0], bounds[3] - bounds[1]) if bounds else 0
    metrics["component_extent_native_pixels"] = extent
    metrics["human_visible"] = (count >= 8 and extent >= 5 and metrics["maximum_exposed_red_gain"] >= 40
                               and metrics["median_exposed_red_gain"] >= 24)
    return metrics, guard, mask_image(exposed, dark.size)


class Suite:
    def __init__(self, args):
        self.args = args
        self.output = args.output.resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.config = self.output / "revived_config/openmr2_setup.cfg"
        self.config.parent.mkdir(exist_ok=True)
        if not self.config.exists():
            for relative in ("build/video-upscale-" + PLATFORM + "/revived_config/openmr2_setup.cfg",
                             "../revived_config/openmr2_setup.cfg"):
                seed = ROOT / relative
                if seed.exists():
                    shutil.copy2(seed, self.config)
                    break
        self.binary = args.binary.resolve()
        self.sha256 = hashlib.sha256(self.binary.read_bytes()).hexdigest()
        if os.name == "nt":
            target = self.output / "OpenMR2.exe"
            shutil.copy2(self.binary, target)
            self.binary = target
        self.baseline_binary = args.baseline_binary.resolve() if args.baseline_binary else None
        if self.baseline_binary and os.name == "nt":
            target = self.output / "OpenMR2-baseline.exe"
            shutil.copy2(self.baseline_binary, target)
            self.baseline_binary = target
        self.results, self.images = [], {}
        self.captures = 0

    def launch(self, name, *, car, quality, fps, scene, after, drive=False,
               lights=True, no_trails=False, retro=False, brake=False, baseline=False):
        receipt = "\n".join(line for line in self.config.read_text().splitlines()
                            if line.startswith(("Files_OK =", "verified_directory =", "verified_language ="))) if self.config.exists() else ""
        settings = {"graphics_mode": int(retro), "startup_mode": int(retro), "remember_startup_mode": 1,
                    "vehicle_lights": int(lights), "light_intensity": 100, "effect_quality": quality,
                    "effects_muted": 0, "frame_rate": "60fps" if fps == 60 else "original",
                    "vehicle_reflections": 0, "real_time_shadows": 0, "damage_effects": 0,
                    "missile_effects": 0, "modern_explosions": 0, "enhanced_oil": 0,
                    "texture_filter": 0, "custom_textures": 0, "video_upscaling": 0,
                    "msaa": 0, "fxaa": 0, "retro_scale": 1, "integer_scale": 0,
                    "retro_output_filter": "point", "sounds_volume": 0, "engines_volume": 0,
                    "race_music_volume": 0, "menu_music_volume": 0, "menu_sounds_volume": 0}
        self.config.write_text(receipt + "\n" + "".join(f"{key} = {value}\n" for key, value in settings.items()))
        shot = self.output / (name + ".bmp")
        shot.unlink(missing_ok=True)
        state_path = self.output / "graphics-state.txt"
        state_path.unlink(missing_ok=True)
        executable = self.baseline_binary if baseline else self.binary
        assert executable, "A baseline binary is required for this capture"
        command = [str(executable), "--game", str((self.args.game_root / "Megarace 2 English").resolve()),
                   "--size", "640x480", "--shot", str(shot), "--shot-after", str(after),
                   "--shotfps", str(fps), "--car", str(car), "--graphics-test", str(scene),
                   "--track", "1", "--frame", "150", "--test-damage", str(int(brake)),
                   "--objtest-drive" if drive else "--objtest"]
        if no_trails:
            command.append("--no-light-trails")
        environment, options = dict(os.environ), {}
        if os.name == "nt":
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
            options["startupinfo"] = startup
        else:
            environment["LIBGL_ALWAYS_SOFTWARE"] = "1"
            command = ["xvfb-run", "-a"] + command
        process = subprocess.run(command, cwd=self.output, env=environment, timeout=300,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **options)
        assert process.returncode == 0 and shot.exists(), (name, process.returncode, process.stdout[-3000:])
        state = {key: int(value) for key, value in
                 (line.split("=", 1) for line in state_path.read_text().splitlines())}
        assert state["gl_errors"] == 0, (name, state)
        image = Image.open(shot).convert("RGB")
        image.save(self.output / (name + ".png"))
        self.images[name] = image
        self.captures += 1
        print(PLATFORM, name, "captured", flush=True)
        return image

    def case(self, name, expected_visible=True, **options):
        dark = self.launch(name + "-dark", lights=False, **options)
        lamps = self.launch(name + "-lamps", no_trails=True, **options)
        trails = self.launch(name + "-trails", **options)
        metrics, guard, exposed = trail_metrics(dark, lamps, trails)
        guard.save(self.output / (name + "-lamp-footprint.png"))
        exposed.save(self.output / (name + "-exposed-mask.png"))
        self.results.append({"case": name, "expected_visible": expected_visible, **options, **metrics})
        if not expected_visible:
            assert lamps.tobytes() == trails.tobytes(), "Trail leaked from a car without rear red lights: " + name
        # Keep the important direct comparison at native pixels, without an
        # amplified difference or resampling that can hide actual visibility.
        comparison = Image.new("RGB", (1280, 500), "#161616")
        comparison.paste(lamps, (0, 20))
        comparison.paste(trails, (640, 20))
        label = ImageDraw.Draw(comparison)
        label.text((4, 4), "Current lamps / no trail", fill="white")
        label.text((644, 4), "Trail enabled", fill="white")
        comparison.save(self.output / (name + "-native-compare.png"))
        print(name, json.dumps(metrics), flush=True)

    def run(self):
        if self.args.group in ("all", "ordinary"):
            for fps in (25, 60):
                self.case(f"slow-forward-{fps}", car=0, quality=2, fps=fps, scene=9, after=round(fps * 1.6))
                self.case(f"ordinary-drive-{fps}", car=0, quality=2, fps=fps, scene=4,
                          after=round(fps * 6), drive=True)
                self.case(f"ordinary-drive-extreme-{fps}", car=0, quality=3, fps=fps, scene=4,
                          after=round(fps * 6), drive=True)
        if self.args.group in ("all", "controlled"):
            for car, fps, quality, brake in ((0, 25, 2, False), (0, 60, 2, False),
                                            (0, 25, 2, True),
                                            (1, 25, 3, False), (1, 60, 3, True),
                                            (8, 25, 2, True), (8, 60, 3, False)):
                self.case(f"chase-car{car:02}-{fps}-q{quality}-brake{int(brake)}", car=car,
                          quality=quality, fps=fps, scene=self.args.scene + int(brake), after=round(fps * 1.6),
                          brake=brake, drive=True)
            self.case("chase-blatt-front-only", car=13, quality=3, fps=60, scene=self.args.scene,
                      after=96, drive=True, expected_visible=False)
            def red_energy(name):
                dark, full = self.images[name + "-dark"], self.images[name + "-trails"]
                return sum(max(0, p[0] - d[0]) for p, d in zip(pixel_data(full), pixel_data(dark))
                           if p[0] > p[1] * 1.4 and p[0] > p[2] * 1.3)
            normal = red_energy("chase-car00-25-q2-brake0")
            braking = red_energy("chase-car00-25-q2-brake1")
            assert braking > normal * 1.2, ("Braking did not brighten the lamps/trail", normal, braking)
            self.results.append({"case": "braking-increases-red-light-energy", "normal_red_energy": normal,
                                 "braking_red_energy": braking})
        if self.args.group in ("all", "preserve"):
            for mode in (False, True):
                options = dict(car=0, quality=1 if not mode else 3, fps=25, scene=9, after=40, retro=mode)
                with_trails = self.launch("preserve-" + str(int(mode)) + "-trails", **options)
                no_trails = self.launch("preserve-" + str(int(mode)) + "-no-trails", no_trails=True, **options)
                if mode:
                    assert with_trails.tobytes() == no_trails.tobytes(), "Trails changed Retro output"
                if self.baseline_binary:
                    reference = self.launch("preserve-" + str(int(mode)) + "-baseline", baseline=True, **options)
                    assert with_trails.tobytes() == reference.tobytes(), "Changed accepted " + ("Retro" if mode else "Medium") + " presentation"
                self.results.append({"case": "retro-identity" if mode else "medium-reference",
                                     "byte_identical_with_no_trails": with_trails.tobytes() == no_trails.tobytes(),
                                     "byte_identical_with_0718": True if self.baseline_binary else None})
        self.save()
        if self.args.phase == "verify":
            failed = [item["case"] for item in self.results
                      if item["case"].startswith(("chase-", "ordinary-drive-"))
                      and item.get("expected_visible") and not item["human_visible"]]
            assert not failed, ("Trails are not visibly exposed at native display size", failed)

    def save(self):
        report = {"platform": PLATFORM, "phase": self.args.phase, "binary_sha256": self.sha256,
                  "captures": self.captures, "results": self.results,
                  "metric_note": "Native 640x480; >=18 red gain and R>=90 outside 2px current-lamp guard; connected >=8px and >=5px extent, peak gain>=40 and median>=24."}
        (self.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        rows = (len(self.images) + 2) // 3
        sheet = Image.new("RGB", (640 * 3, rows * 505), "#161616")
        draw = ImageDraw.Draw(sheet)
        for index, (name, image) in enumerate(self.images.items()):
            x, y = index % 3 * 640, index // 3 * 505
            draw.text((x + 4, y + 4), name, fill="white")
            sheet.paste(image, (x, y + 22))
        sheet.save(self.output / "review-native.png")
        print(PLATFORM, self.captures, "captures; report", self.output / "results.json", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--baseline-binary", type=Path, help="Accepted 0.7.18 for byte-identical Medium/Retro checks")
    parser.add_argument("--game-root", type=Path, default=ROOT.parents[1])
    parser.add_argument("--output", type=Path, default=ROOT / "build" / ("trail-0719-" + PLATFORM))
    parser.add_argument("--phase", choices=("baseline", "verify"), default="verify")
    parser.add_argument("--group", choices=("all", "ordinary", "controlled", "preserve"), default="all")
    parser.add_argument("--scene", type=int, default=23,
                        help="Controlled normal-forward/chase fixture; scene+1 displays braking on the same trajectory.")
    Suite(parser.parse_args()).run()


if __name__ == "__main__":
    main()
