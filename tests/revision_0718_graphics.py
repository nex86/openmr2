"""Real GPU captures for Revived reflections, effect quality and Retro isolation.

Runs on Windows or Linux (xvfb-run + Mesa). All configuration and captures stay
under build/. An optional pre-0.7.18 binary checks the accepted Medium appearance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
PLATFORM = "windows" if os.name == "nt" else "linux"


class Suite:
    def __init__(self, args):
        self.args = args
        self.run = args.output.resolve()
        self.run.mkdir(parents=True, exist_ok=True)
        self.config = self.run / "revived_config/openmr2_setup.cfg"
        self.config.parent.mkdir(exist_ok=True)
        self.images, self.states, self.checks = {}, {}, []
        self.captures = 0

    def launch(self, name, *, reflections=0, quality=1, mode=0, msaa=0,
               screen=None, scene=4, damage=0, after=2, lights=0, shadows=0,
               effects=0, age=150, no_trails=False, no_fire=False, custom=0, language="English", baseline=False):
        receipt = "\n".join(line for line in self.config.read_text().splitlines()
                            if line.startswith(("Files_OK =", "verified_directory =", "verified_language ="))) if self.config.exists() else ""
        options = {
            "graphics_mode": mode, "startup_mode": mode, "remember_startup_mode": 1,
            "vehicle_reflections": reflections, "reflection_intensity": 100,
            "effect_quality": quality, "effects_muted": 0, "frame_rate": "original",
            "texture_filter": 0, "retro_scale": 1, "integer_scale": 0,
            "retro_output_filter": "point", "msaa": msaa, "fxaa": 0,
            "video_upscaling": 0, "vehicle_lights": lights, "real_time_shadows": shadows,
            "custom_textures": custom,
            "damage_effects": effects, "enhanced_oil": effects,
            "modern_explosions": effects, "missile_effects": effects,
            "shadow_intensity": 100, "light_intensity": 100, "oil_intensity": 100,
            "damage_intensity": 100, "explosion_intensity": 100, "missile_intensity": 100,
            "sounds_volume": 0, "engines_volume": 0, "race_music_volume": 0,
            "menu_music_volume": 0, "menu_sounds_volume": 0,
        }
        self.config.write_text(receipt + "\n" + "".join(f"{key} = {value}\n" for key, value in options.items()))
        executable = self.args.baseline if baseline else self.args.binary
        if os.name == "nt":
            target = self.run / "OpenMR2.exe"
            shutil.copy2(executable, target)
            executable = target
        shot = self.run / (name + ".bmp")
        shot.unlink(missing_ok=True)
        state_path = self.run / "graphics-state.txt"
        state_path.unlink(missing_ok=True)
        command = [str(executable.resolve()), "--game", str((self.args.game_root / ("Megarace 2 " + language)).resolve()),
                   "--size", "1280x960" if screen == "effects" else "640x480",
                   "--shot", str(shot), "--shot-after", str(after), "--shotfps", "25",
                   "--car", "0", "--graphics-test", str(scene), "--effect-age-ms", str(age),
                   "--test-damage", str(damage)]
        if no_trails:
            command.append("--no-light-trails")
        if no_fire:
            command.append("--no-engine-fire")
        if screen:
            command += ["--screen", screen]
        else:
            command += ["--track", "1", "--frame", "150", "--objtest"]
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
        process = subprocess.run(command, cwd=self.run, env=environment, timeout=300,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
        assert process.returncode == 0 and shot.exists(), (name, process.returncode, process.stdout[-3000:])
        image = Image.open(shot).convert("RGB")
        image.save(self.run / (name + ".png"))
        self.images[name] = image
        self.captures += 1
        if not baseline:
            assert state_path.exists(), (name, "Missing explicit developer graphics diagnostics")
            state = {key: int(value) for key, value in (line.split("=", 1) for line in state_path.read_text().splitlines())}
            assert state["gl_errors"] == 0, (name, state)
            assert state["reflection_failed"] == 0, (name, state)
            self.states[name] = state
            (self.run / (name + "-state.json")).write_text(json.dumps(state, indent=2))
        print(PLATFORM, name, "captured", flush=True)
        return image

    def check(self, text, **details):
        self.checks.append({"check": text, "pass": True, **details})

    def reflections(self):
        off = self.launch("race-reflections-off")
        on = self.launch("race-reflections-on", reflections=1)
        changed = different(off, on)
        assert changed > 20, ("Race reflection material did not change", changed)
        state = self.states["race-reflections-on"]
        assert state["reflection_valid"] == 1 and state["reflection_captures"] > 0 and state["reflection_draws"] > 0
        assert state["shadow_map_size"] == 0, "Reflections unexpectedly depended on enabled shadows"
        assert off.crop((0, 0, 640, 100)).tobytes() == on.crop((0, 0, 640, 100)).tobytes(), "Reflection pass changed the HUD"
        assert off.crop((0, 395, 640, 480)).tobytes() == on.crop((0, 395, 640, 480)).tobytes(), "Reflection pass changed pixels below the track backplate"
        self.check("Race environment reflections are visible with shadows disabled; HUD unchanged", changed_pixels=changed)
        self.launch("race-reflections-msaa4", reflections=1, msaa=4)
        assert self.states["race-reflections-msaa4"]["reflection_captures"] > 0
        self.check("Multisampled backplate resolves into reflection capture without GL errors/fallback")
        off = self.launch("preview-reflections-off", screen="carshop", scene=14, age=1571)
        on = self.launch("preview-reflections-on", screen="carshop", scene=14, age=1571, reflections=1)
        assert different(off, on) > 20, "Preview reflections missing"
        assert self.states["preview-reflections-on"]["reflection_captures"] > 0
        outside = ImageChops.difference(off, on)
        outside.paste((0, 0, 0), (130, 84, 510, 284))
        assert outside.getbbox() is None, "Preview reflections altered pixels outside the vehicle viewport"
        self.check("Career preview reflects its clean backdrop independently of shadows; change stays inside viewport")
        for screen, scene in ((None, 6), ("carshop", 14)):
            label = "race" if screen is None else "preview"
            off = self.launch("retro-" + label + "-off", screen=screen, scene=scene, mode=1, damage=95)
            on = self.launch("retro-" + label + "-all-on", screen=screen, scene=scene, mode=1,
                             damage=95, reflections=1, quality=3, lights=1, shadows=1, effects=1)
            assert off.tobytes() == on.tobytes(), "Modern effects escaped Retro locks: " + label
            state = self.states["retro-" + label + "-all-on"]
            assert state["reflection_captures"] == 0 and state["reflection_map_size"] == 0
            self.check("Retro " + label + " remains pixel-identical and allocates no reflection map")

    def quality(self):
        for quality, size in ((0, 1024), (1, 2048), (2, 3072), (3, 4096)):
            name = "shadow-quality-" + str(quality)
            self.launch(name, shadows=1, quality=quality)
            assert self.states[name]["shadow_map_size"] == size, (name, self.states[name])
        self.check("Shared quality selects 1024/2048/3072/4096 shadow maps without GL errors")
        oil_medium = self.launch("medium-oil-quality", scene=2, effects=1, after=40)
        oil_high = self.launch("high-oil-quality", scene=2, effects=1, quality=2, after=40)
        oil_extreme = self.launch("extreme-oil-quality", scene=2, effects=1, quality=3, after=40)
        high_detail, extreme_detail = different(oil_medium, oil_high), different(oil_high, oil_extreme)
        assert 2 < high_detail < 640 * 480 // 3 and 0 < extreme_detail < 640 * 480 // 3, ("Oil quality detail missing/unstable", high_detail, extreme_detail)
        self.check("High/Extreme thin-film oil detail changes the localized material without GL errors",
                   high_detail_pixels=high_detail, extreme_detail_pixels=extreme_detail)
        medium_fire = self.launch("medium-fire-reference", scene=6, damage=95, quality=1, effects=1, after=40)
        medium_off = self.launch("medium-fire-reference-off", scene=6, damage=95, quality=1, effects=1, after=40, no_fire=True)
        medium_pixels, medium_bounds = fire_coverage(medium_fire, medium_off)
        assert medium_pixels > 4 and medium_bounds is not None, "Medium reference lacks visible vehicle fire"
        medium_height = medium_bounds[3] - medium_bounds[1]
        for quality, label in ((2, "high"), (3, "extreme")):
            off = self.launch(label + "-moving-trails-off", scene=21, quality=quality, lights=1, after=40, no_trails=True)
            on = self.launch(label + "-moving-trails-on", scene=21, quality=quality, lights=1, after=40)
            changed = different(off, on)
            assert changed > 2, (label, "No continuous moving tail-light trail", changed)
            assert changed < 640 * 480 // 3, (label, "Trail unexpectedly contaminated the whole framebuffer", changed)
            self.check(label + " actual moving-history lamp trails are visible and localized", changed_pixels=changed)
            # Race screenshots wait for at least one simulated second before
            # they become ready; use later counts to select distinct times.
            fire = self.launch(label + "-fire-a", scene=6, damage=95, quality=quality, effects=1, after=40)
            later = self.launch(label + "-fire-b", scene=6, damage=95, quality=quality, effects=1, after=41)
            repeat = self.launch(label + "-fire-repeat", scene=6, damage=95, quality=quality, effects=1, after=40)
            suppressed = self.launch(label + "-fire-suppressed", scene=6, damage=95, quality=quality, effects=1, after=40, no_fire=True)
            warm_pixels, warm_bounds = fire_coverage(fire, suppressed)
            assert warm_bounds is not None and warm_pixels >= max(5, medium_pixels * .6), (label, "Vehicle flame coverage regressed", warm_pixels, medium_pixels)
            height = warm_bounds[3] - warm_bounds[1]
            assert height >= max(3, medium_height * .65), (label, "Vehicle flames collapsed to an ember", height, medium_height)
            motion = different(fire, later)
            assert motion > 2 and motion < 640 * 480 // 3, (label, "Fire animation missing or unstable across whole frame", motion)
            assert fire.tobytes() == repeat.tobytes(), (label, "Identical animation state changed across runs")
            assert len(fire.getcolors(640 * 480 + 1)) > 1000, (label, "Rendered frame collapsed to flat/black output")
            self.check(label + " fire remains visibly textured, moves over adjacent frames and repeats deterministically",
                       motion_pixels=motion, flame_pixels=warm_pixels, flame_height=height,
                       medium_flame_pixels=medium_pixels, medium_flame_height=medium_height)
            comparison = Image.new("RGB", (840, 3 * 680), "#161616")
            draw = ImageDraw.Draw(comparison)
            for index, (caption, image) in enumerate((("Medium: accepted appearance", medium_fire),
                                                     (label + ": fire and smoke", fire),
                                                     (label + ": smoke with engine fire suppressed", suppressed))):
                draw.text((5, index * 680 + 3), caption, fill="white")
                comparison.paste(image.crop((300, 150, 440, 260)).resize((840, 660)), (0, index * 680 + 20))
            comparison.save(self.run / (label + "-fire-review.png"))
            moving_fire = self.launch(label + "-fast-fire", scene=22, damage=95, quality=quality, effects=1, after=40)
            moving_off = self.launch(label + "-fast-fire-suppressed", scene=22, damage=95, quality=quality, effects=1, after=40, no_fire=True)
            moving_pixels, moving_bounds = fire_coverage(moving_fire, moving_off)
            assert moving_pixels > 5 and moving_bounds is not None, (label, "Fire disappeared under fast actual vehicle movement", moving_pixels)
            self.check(label + " moving vehicle retains visible connected fire", flame_pixels=moving_pixels,
                       flame_bounds=moving_bounds)

    def baseline(self):
        if not self.args.baseline or not self.args.baseline.exists():
            print("No previous-version native binary supplied; Medium cross-version image check skipped", flush=True)
            return
        for scene, label, options in ((4, "shadows", {"shadows": 1}),
                                      (2, "oil", {"effects": 1}),
                                      (6, "fire", {"effects": 1, "damage": 95})):
            current = self.launch("medium-0718-" + label, scene=scene, **options)
            previous = self.launch("medium-0717-" + label, scene=scene, baseline=True, **options)
            delta = ImageChops.difference(current, previous)
            maximum = max(channel[1] for channel in delta.getextrema())
            changed = different(current, previous)
            assert changed == 0, (label, "Medium appearance changed relative to accepted 0.7.17", changed, maximum)
            self.check("Medium " + label + " retains accepted previous-version output with new reflections off",
                       changed_pixels_over_two_levels=changed, max_channel_difference=maximum,
                       byte_identical=current.tobytes() == previous.tobytes())

    def ui(self):
        for language, quality in (("German", 0), ("English", 1), ("French", 3)):
            image = self.launch("effects-ui-" + language, screen="effects", scene=4,
                                reflections=1, quality=quality, language=language)
            # Exact solid text interiors are distinguishable from the textured
            # background; antialiasing edge pixels are intentionally excluded.
            def ink(box):
                return sum(all(abs(value - target) <= 2 for value, target in zip(pixel, (216, 200, 160)))
                           for pixel in image.crop(box).getdata())
            assert ink((132, 104, 700, 588)) > 100, (language, "Effect labels absent")
            assert ink((132, 680, 700, 734)) > 10, (language, "Quality selector label absent")
            assert ink((1190, 104, 1256, 588)) > 20, (language, "Intensity values clipped/missing")
            assert ink((1260, 100, 1280, 735)) == 0, (language, "Text reaches/clips the screen edge")
            self.check(language + " seven-effect sliders, intensity values and quality controls render within bounds")

    def alpha(self):
        fixture = self.run / "revived_config/texture_replacements/LOCAL/DATA640/CARSHOP.HLZ.png"
        fixture.parent.mkdir(parents=True, exist_ok=True)
        previous = fixture.read_bytes() if fixture.exists() else None
        try:
            for label, color in (("magenta", (255, 0, 255, 0)), ("green", (0, 255, 0, 0))):
                Image.new("RGBA", (640, 480), color).save(fixture)
                self.launch("transparent-" + label + "-off", screen="carshop", scene=14, age=1571, custom=1)
                self.launch("transparent-" + label + "-on", screen="carshop", scene=14, age=1571, custom=1, reflections=1)
            off = different(self.images["transparent-magenta-off"], self.images["transparent-green-off"])
            on = different(self.images["transparent-magenta-on"], self.images["transparent-green-on"])
            print("Transparent hidden-RGB changed pixels: reflections off", off, "; on", on, flush=True)
            assert off == 0, "The ordinary canvas exposed hidden RGB behind alpha-zero pixels"
            assert on == 0, "Vehicle reflections exposed hidden RGB behind alpha-zero pixels"
            self.check("Transparent custom-preview colors are composited before reflection mipmapping; hidden RGB stays invisible")
        finally:
            if previous is None:
                fixture.unlink(missing_ok=True)
            else:
                fixture.write_bytes(previous)

    def save(self):
        names = list(self.images)
        cell_w, cell_h = 640, 505
        sheet = Image.new("RGB", (cell_w * 2, cell_h * ((len(names) + 1) // 2)), "#151515")
        draw = ImageDraw.Draw(sheet)
        for index, name in enumerate(names):
            x, y = index % 2 * cell_w, index // 2 * cell_h
            draw.text((x + 5, y + 4), name, fill="white")
            sheet.paste(self.images[name].resize((640, 480)), (x, y + 23))
        sheet.save(self.run / "review.png")
        report = {"platform": PLATFORM, "captures": self.captures, "checks": self.checks,
                  "states": self.states, "binary_sha256": hashlib.sha256(self.args.binary.read_bytes()).hexdigest(),
                  "failures": 0}
        (self.run / "results.json").write_text(json.dumps(report, indent=2) + "\n")
        (ROOT / "build" / (PLATFORM + "-revision-0718-results.json")).write_text(json.dumps(report, indent=2) + "\n")
        print(PLATFORM, self.captures, "GPU captures,", len(self.checks), "checks passed", flush=True)


def different(a, b):
    return sum(max(pixel) > 2 for pixel in ImageChops.difference(a, b).getdata())


def fire_coverage(on, off):
    """Count the isolated warm flame layer, excluding unchanged paint/scenery."""
    changed = ImageChops.difference(on, off)
    points = []
    width = on.width
    for index, (pixel, delta) in enumerate(zip(on.getdata(), changed.getdata())):
        red, green, blue = pixel
        if max(delta) > 5 and red > 90 and green > 35 and red > blue * 1.3 and green > blue * 1.15:
            points.append((index % width, index // width))
    if not points:
        return 0, None
    return len(points), (min(p[0] for p in points), min(p[1] for p in points),
                         max(p[0] for p in points) + 1, max(p[1] for p in points) + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build" / ("OpenMR2.exe" if os.name == "nt" else "openmr2_linux"))
    parser.add_argument("--baseline", type=Path, default=ROOT / "build/backups/OpenMR2-before-0.7.18.exe" if os.name == "nt" else None)
    parser.add_argument("--game-root", type=Path, default=ROOT.parents[1])
    parser.add_argument("--output", type=Path, default=ROOT / "build" / ("revision-0718-" + PLATFORM))
    parser.add_argument("--group", choices=("all", "reflections", "quality", "baseline", "ui", "alpha"), default="all")
    args = parser.parse_args()
    suite = Suite(args)
    for group in ("reflections", "quality", "baseline", "ui", "alpha"):
        if args.group in ("all", group):
            getattr(suite, group)()
    suite.save()


if __name__ == "__main__":
    main()
