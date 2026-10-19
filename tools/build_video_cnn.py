"""Embed the licensed FSRCNNX release 1.1 models as OpenGL 3.3 fragment passes.

Regeneration is offline: the original model files and licenses live under
src/third_party/fsrcnnx. No model weights or tensor arithmetic are retrained or
approximated here. The mpv hook/sampler interface is adapted to numbered textures.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "src/third_party/fsrcnnx"
DESTINATION = ROOT / "src/video_cnn_data.h"
MODELS = ("FSRCNNX_x2_8-0-4-1.glsl", "FSRCNNX_x2_16-0-4-1.glsl")


@dataclass(frozen=True)
class Hook:
    description: str
    bindings: tuple[str, ...]
    save: str
    scale: int
    code: str


def parse_model(text: str) -> tuple[str, list[Hook]]:
    """Read the deliberately small subset of mpv hook syntax used by the models."""
    sections = re.split(r"^//!HOOK LUMA\s*$", text, flags=re.MULTILINE)
    if len(sections) == 1:
        raise ValueError("FSRCNNX source contains no LUMA hooks")
    license_header, *sections = sections
    hooks: list[Hook] = []
    for index, section in enumerate(sections):
        metadata: dict[str, list[str]] = {}
        body: list[str] = []
        for line in section.splitlines():
            if line.startswith("//!"):
                key, _, value = line[3:].partition(" ")
                metadata.setdefault(key, []).append(value.strip())
            else:
                body.append(line)
        allowed = {"WHEN", "DESC", "BIND", "SAVE", "COMPONENTS", "WIDTH", "HEIGHT"}
        if set(metadata) - allowed:
            raise ValueError(f"Unsupported model directives: {set(metadata) - allowed}")
        if metadata.get("WHEN") != ["OUTPUT.w LUMA.w / 1.300 > OUTPUT.h LUMA.h / 1.300 > *"]:
            raise ValueError("Unexpected upstream scaling condition")
        dimensions = (metadata.get("WIDTH"), metadata.get("HEIGHT"))
        if dimensions == (None, None):
            scale = 1
        elif dimensions == (["LUMA.w 2 *"], ["LUMA.h 2 *"]):
            scale = 2
        else:
            raise ValueError(f"Unsupported model dimensions: {dimensions}")
        if metadata.get("COMPONENTS", ["4"]) != ["4"]:
            raise ValueError("Expected four-component feature tensors")
        saves = metadata.get("SAVE", [])
        if len(saves) > 1 or (not saves and index != len(sections) - 1):
            raise ValueError("Only the final aggregation may omit SAVE")
        if (scale == 2) != (index == len(sections) - 1):
            raise ValueError("Only the final aggregation may resize its output")
        hooks.append(Hook(
            metadata["DESC"][0], tuple(metadata.get("BIND", [])),
            saves[0] if saves else "OUTPUT_LUMA", scale,
            "\n".join(body).strip() + "\n",
        ))
    return license_header.strip() + "\n", hooks


def luma_scalars(code: str) -> str:
    """Extract the first component explicitly from adapted vec4 LUMA fetches."""
    pattern = re.compile(r"\bfloat\(\s*LUMA_tex(?:Off)?\(")
    while match := pattern.search(code):
        start = code.index("(", match.start())
        depth = 1
        end = start + 1
        while end < len(code) and depth:
            depth += (code[end] == "(") - (code[end] == ")")
            end += 1
        if depth:
            raise ValueError("Unbalanced scalar LUMA expression")
        expression = code[start + 1:end - 1].strip()
        code = code[:match.start()] + "(" + expression + ").r" + code[end:]
    return code


def fragment_shader(header: str, hook: Hook) -> str:
    lines = ["#version 330 core", header.rstrip(),
             "// OpenMR2 adapter: clamped texel fetches; upstream tensor arithmetic unchanged.",
             "uniform vec2 uOutput;", "out vec4 o;"]
    lines.extend(f"uniform sampler2D uBind{i};" for i in range(6))
    lines.append("vec2 cnnPos(){return gl_FragCoord.xy/uOutput;}")
    for index, name in enumerate(hook.bindings):
        lines.extend([
            f"vec4 cnnRead{index}(vec2 pos){{",
            f"    ivec2 size=textureSize(uBind{index},0);",
            "    ivec2 pixel=clamp(ivec2(floor(pos*vec2(size))),ivec2(0),size-ivec2(1));",
            f"    vec4 value=texelFetch(uBind{index},pixel,0);",
        ])
        if name == "LUMA":
            lines.append("    return vec4(dot(value.rgb,vec3(0.299,0.587,0.114)),0.0,0.0,1.0);")
        else:
            lines.append("    return value;")
        lines.extend([
            "}",
            f"#define {name}_size vec2(textureSize(uBind{index},0))",
            f"#define {name}_pt (vec2(1.0)/{name}_size)",
            f"#define {name}_pos cnnPos()",
            f"#define {name}_tex(pos) cnnRead{index}(pos)",
            f"#define {name}_texOff(off) cnnRead{index}({name}_pos+vec2(off)*{name}_pt)",
        ])
    code = luma_scalars(hook.code)
    used_names = set(re.findall(r"\b([A-Z][A-Z0-9]*)_(?:texOff|tex|pos|size|pt)\b", code))
    if used_names != set(hook.bindings):
        raise ValueError(f"Sampler declarations differ from sampler use in {hook.description}")
    lines.extend([code.rstrip(), "void main(){o=hook();}", ""])
    return "\n".join(lines)


def schedule(hooks: list[Hook]) -> tuple[list[tuple[Hook, tuple[int, ...], int]], int, int]:
    """Allocate stable slots; repeated SAVE names reuse their existing texture."""
    slots = {"LUMA": 0}
    written = {0}
    result: list[tuple[Hook, tuple[int, ...], int]] = []
    for hook in hooks:
        if not 0 < len(hook.bindings) <= 6 or len(set(hook.bindings)) != len(hook.bindings):
            raise ValueError(f"Unsupported binding count/duplicates: {hook.description}")
        unknown = set(hook.bindings) - slots.keys()
        if unknown:
            raise ValueError(f"Read before write in {hook.description}: {sorted(unknown)}")
        bindings = tuple(slots[name] for name in hook.bindings)
        output = slots.setdefault(hook.save, len(slots))
        if output in bindings or output == 0:
            raise ValueError(f"Read/write feedback in {hook.description}")
        if not set(bindings) <= written:
            raise ValueError(f"Uninitialized input in {hook.description}")
        written.add(output)
        result.append((hook, bindings, output))
    return result, len(slots), result[-1][2]


def generate() -> tuple[str, list[dict[str, int | str]]]:
    provenance = json.loads((VENDOR / "provenance.json").read_text(encoding="utf-8"))
    output = ["// Generated by tools/build_video_cnn.py; do not edit by hand.",
              "// FSRCNNX Copyright (C) 2017-2021 igv, LGPL-3.0-or-later.",
              "// Complete corresponding model source and licenses: third_party/fsrcnnx/.",
              "#ifndef OPENMR2_VIDEO_CNN_DATA_H", "#define OPENMR2_VIDEO_CNN_DATA_H", "",
              "struct VideoCnnPass { const char *fragment; int bindings[6]; int binding_count; int output; int scale; };",
              "struct VideoCnnModel { const VideoCnnPass *passes; int count; int slots; int final_slot; };", ""]
    models: list[tuple[int, int, int]] = []
    summary: list[dict[str, int | str]] = []
    for model_index, filename in enumerate(MODELS):
        data = (VENDOR / filename).read_bytes()
        if hashlib.sha256(data).hexdigest() != provenance["files"][filename]["sha256"]:
            raise ValueError(f"Vendored model hash mismatch: {filename}")
        header, hooks = parse_model(data.decode("utf-8"))
        passes, count, final_slot = schedule(hooks)
        for pass_index, (hook, _, _) in enumerate(passes):
            shader = fragment_shader(header, hook)
            if ')cnn"' in shader:
                raise ValueError("Shader collides with C++ raw-string delimiter")
            output.extend([
                f"// {filename}: {hook.description}",
                f'static const char VIDEO_CNN_{model_index}_{pass_index}[]=R"cnn({shader})cnn";', "",
            ])
        output.append(f"static const VideoCnnPass VIDEO_CNN_PASSES_{model_index}[]={{")
        for pass_index, (hook, bindings, slot) in enumerate(passes):
            padded = ",".join(map(str, bindings + (0,) * (6 - len(bindings))))
            output.append(f"    {{VIDEO_CNN_{model_index}_{pass_index},{{{padded}}},{len(bindings)},{slot},{hook.scale}}},")
        output.extend(["};", ""])
        models.append((len(passes), count, final_slot))
        summary.append({"model": filename, "passes": len(passes), "slots": count, "final_slot": final_slot})
    output.append("static const VideoCnnModel VIDEO_CNN_MODELS[2]={")
    for index, (passes, count, final_slot) in enumerate(models):
        output.append(f"    {{VIDEO_CNN_PASSES_{index},{passes},{count},{final_slot}}},")
    output.extend(["};", "", "#endif", ""])
    return "\n".join(output), summary


def self_test() -> None:
    """Exercise scheduler hazards and subpixel-channel indexing without a GPU."""
    hook = lambda binds, save: Hook("test", tuple(binds), save, 1, "")
    sequence = [hook(["LUMA"], "A"), hook(["A"], "B"), hook(["B"], "A")]
    passes, slots, final = schedule(sequence)
    assert slots == 3 and final == 1
    assert [item[2] for item in passes] == [1, 2, 1]
    for invalid in ([hook(["LUMA"], "LUMA")], [hook(["MISSING"], "A")],
                    [hook(["LUMA"], "A"), hook(["A"], "A")]):
        try:
            schedule(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Scheduler accepted uninitialized input or framebuffer feedback")
    assert luma_scalars("float(LUMA_texOff(vec2(-2,-1)))") == "(LUMA_texOff(vec2(-2,-1))).r"
    # Upstream's aggregation maps each 2x2 output tile to channels [0,2;1,3].
    for width, height in ((1, 1), (3, 5), (320, 200), (640, 400)):
        for x, y in ((0, 0), (1, 0), (0, 1), (1, 1), (2*width-1, 2*height-1)):
            source_x, source_y = (x + 0.5) / 2, (y + 0.5) / 2
            phase_x, phase_y = int((source_x % 1) * 2), int((source_y % 1) * 2)
            assert phase_x * 2 + phase_y == (x % 2) * 2 + y % 2


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify the checked-in generated header")
    parser.add_argument("--self-test", action="store_true", help="also exercise allocation hazards and subpixel phases")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    text, summary = generate()
    if args.check:
        if not DESTINATION.exists() or DESTINATION.read_text(encoding="utf-8") != text:
            raise SystemExit("video_cnn_data.h is stale; run tools/build_video_cnn.py")
    else:
        DESTINATION.write_text(text, encoding="utf-8", newline="\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
