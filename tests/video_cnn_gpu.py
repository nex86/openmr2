"""Check FSRCNNX adapters on a real headless OpenGL 3.3 driver.

Linux: python3 tests/video_cnn_gpu.py
Uses only Python's standard library and system libEGL/libGL (no GL headers,
window manager, downloaded model, PyOpenGL, or image packages). Mesa software
rendering is the default so the reference check does not require a GPU.
"""
from __future__ import annotations

import ctypes as C
import importlib.util
import json
import math
import os
from pathlib import Path
import sys

os.environ.setdefault("LIBGL_ALWAYS_SOFTWARE", "1")
os.environ.setdefault("MESA_GL_VERSION_OVERRIDE", "3.3")
os.environ.setdefault("MESA_GLSL_VERSION_OVERRIDE", "330")

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("video_cnn_builder", ROOT / "tools/build_video_cnn.py")
builder = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = builder
spec.loader.exec_module(builder)

U, I, F, P = C.c_uint, C.c_int, C.c_float, C.c_void_p
RGBA, FLOAT, RGBA16F = 0x1908, 0x1406, 0x881A
TEXTURE_2D, FRAMEBUFFER = 0x0DE1, 0x8D40


class HeadlessGL:
    def __init__(self):
        self.egl = C.CDLL("libEGL.so.1")
        self.egl.eglGetProcAddress.argtypes = [C.c_char_p]
        self.egl.eglGetProcAddress.restype = P
        for name, result, arguments in (
            ("eglGetError", U, []),
            ("eglInitialize", U, [P, C.POINTER(I), C.POINTER(I)]),
            ("eglBindAPI", U, [U]),
            ("eglChooseConfig", U, [P, C.POINTER(I), C.POINTER(P), I, C.POINTER(I)]),
            ("eglCreatePbufferSurface", P, [P, P, C.POINTER(I)]),
            ("eglCreateContext", P, [P, P, P, C.POINTER(I)]),
            ("eglMakeCurrent", U, [P, P, P, P]),
        ):
            function = getattr(self.egl, name)
            function.restype, function.argtypes = result, arguments
        platform_address = self.egl.eglGetProcAddress(b"eglGetPlatformDisplayEXT")
        if not platform_address:
            raise RuntimeError("EGL_EXT_platform_base is required for surfaceless testing")
        platform = C.CFUNCTYPE(P, U, P, C.POINTER(I))(platform_address)
        self.display = platform(0x31DD, None, None)  # EGL_PLATFORM_SURFACELESS_MESA
        major, minor = I(), I()
        self.require(self.egl.eglInitialize(self.display, C.byref(major), C.byref(minor)), "eglInitialize")
        self.require(self.egl.eglBindAPI(0x30A2), "eglBindAPI(OpenGL)")
        attributes = (I * 13)(0x3024, 8, 0x3023, 8, 0x3022, 8,
                              0x3033, 1, 0x3040, 8, 0x3021, 8, 0x3038)
        configuration, count = P(), I()
        self.require(self.egl.eglChooseConfig(self.display, attributes, C.byref(configuration), 1, C.byref(count))
                     and count.value, "eglChooseConfig")
        surface_attributes = (I * 5)(0x3057, 16, 0x3056, 16, 0x3038)
        self.surface = self.egl.eglCreatePbufferSurface(self.display, configuration, surface_attributes)
        self.require(self.surface, "eglCreatePbufferSurface")
        context_attributes = (I * 7)(0x3098, 3, 0x30FB, 3, 0x30FD, 1, 0x3038)
        self.context = self.egl.eglCreateContext(self.display, configuration, None, context_attributes)
        self.require(self.context, "eglCreateContext(3.3 core)")
        self.require(self.egl.eglMakeCurrent(self.display, self.surface, self.surface, self.context), "eglMakeCurrent")
        self.functions()
        vao = U()
        self.glGenVertexArrays(1, C.byref(vao))
        self.glBindVertexArray(vao)
        framebuffer = U()
        self.glGenFramebuffers(1, C.byref(framebuffer))
        self.framebuffer = framebuffer.value
        self.glBindFramebuffer(FRAMEBUFFER, self.framebuffer)
        self.glPixelStorei(0x0CF5, 1)  # GL_UNPACK_ALIGNMENT
        self.glPixelStorei(0x0D05, 1)  # GL_PACK_ALIGNMENT

    def require(self, value, operation):
        if not value:
            raise RuntimeError(f"{operation} failed: EGL error {self.egl.eglGetError():#x}")

    def functions(self):
        declarations = {
            "glGetString": (C.c_char_p, [U]), "glGetError": (U, []),
            "glGenVertexArrays": (None, [I, C.POINTER(U)]), "glBindVertexArray": (None, [U]),
            "glGenFramebuffers": (None, [I, C.POINTER(U)]), "glBindFramebuffer": (None, [U, U]),
            "glFramebufferTexture2D": (None, [U, U, U, U, I]), "glCheckFramebufferStatus": (U, [U]),
            "glGenTextures": (None, [I, C.POINTER(U)]), "glDeleteTextures": (None, [I, C.POINTER(U)]),
            "glBindTexture": (None, [U, U]), "glTexParameteri": (None, [U, U, I]),
            "glTexImage2D": (None, [U, I, I, I, I, I, U, U, P]),
            "glGetTexLevelParameteriv": (None, [U, I, U, C.POINTER(I)]),
            "glActiveTexture": (None, [U]), "glPixelStorei": (None, [U, I]),
            "glCreateShader": (U, [U]), "glShaderSource": (None, [U, I, C.POINTER(C.c_char_p), C.POINTER(I)]),
            "glCompileShader": (None, [U]), "glGetShaderiv": (None, [U, U, C.POINTER(I)]),
            "glGetShaderInfoLog": (None, [U, I, C.POINTER(I), C.c_char_p]), "glDeleteShader": (None, [U]),
            "glCreateProgram": (U, []), "glAttachShader": (None, [U, U]), "glLinkProgram": (None, [U]),
            "glGetProgramiv": (None, [U, U, C.POINTER(I)]),
            "glGetProgramInfoLog": (None, [U, I, C.POINTER(I), C.c_char_p]), "glDeleteProgram": (None, [U]),
            "glUseProgram": (None, [U]), "glGetUniformLocation": (I, [U, C.c_char_p]),
            "glUniform1i": (None, [I, I]), "glUniform2f": (None, [I, F, F]),
            "glViewport": (None, [I, I, I, I]), "glDrawArrays": (None, [U, I, I]),
            "glReadPixels": (None, [I, I, I, I, U, U, P]),
        }
        for name, (result, arguments) in declarations.items():
            address = self.egl.eglGetProcAddress(name.encode())
            if not address:
                raise RuntimeError(f"Missing OpenGL 3.3 function: {name}")
            setattr(self, name, C.CFUNCTYPE(result, *arguments)(address))

    def program(self, fragment):
        vertex = """#version 330 core
void main(){vec2 p=vec2(float(gl_VertexID&1),float((gl_VertexID>>1)&1));gl_Position=vec4(p*2.0-1.0,0,1);}
"""
        program = self.glCreateProgram()
        for shader_type, code in ((0x8B31, vertex), (0x8B30, fragment)):
            shader = self.glCreateShader(shader_type)
            source = C.c_char_p(code.encode())
            self.glShaderSource(shader, 1, C.byref(source), None)
            self.glCompileShader(shader)
            success = I()
            self.glGetShaderiv(shader, 0x8B81, C.byref(success))
            if not success.value:
                message = C.create_string_buffer(16384)
                self.glGetShaderInfoLog(shader, len(message), None, message)
                raise AssertionError(message.value.decode(errors="replace"))
            self.glAttachShader(program, shader)
            self.glDeleteShader(shader)
        self.glLinkProgram(program)
        success = I()
        self.glGetProgramiv(program, 0x8B82, C.byref(success))
        if not success.value:
            message = C.create_string_buffer(16384)
            self.glGetProgramInfoLog(program, len(message), None, message)
            raise AssertionError(message.value.decode(errors="replace"))
        return program

    def texture(self, width, height, data=None):
        texture = U()
        self.glGenTextures(1, C.byref(texture))
        self.upload(texture.value, width, height, data)
        return texture.value

    def upload(self, texture, width, height, data=None):
        self.glBindTexture(TEXTURE_2D, texture)
        self.glTexParameteri(TEXTURE_2D, 0x2801, 0x2600)  # nearest min/mag
        self.glTexParameteri(TEXTURE_2D, 0x2800, 0x2600)
        self.glTexParameteri(TEXTURE_2D, 0x2802, 0x812F)  # clamp to edge
        self.glTexParameteri(TEXTURE_2D, 0x2803, 0x812F)
        values = (F * len(data))(*data) if data is not None else None
        self.glTexImage2D(TEXTURE_2D, 0, RGBA16F, width, height, 0, RGBA, FLOAT, values)
        actual = I()
        self.glGetTexLevelParameteriv(TEXTURE_2D, 0, 0x1003, C.byref(actual))
        assert actual.value == RGBA16F, "CNN texture lost its signed FP16 format"

    def render(self, program, textures, destination, width, height):
        self.glBindFramebuffer(FRAMEBUFFER, self.framebuffer)
        self.glFramebufferTexture2D(FRAMEBUFFER, 0x8CE0, TEXTURE_2D, destination, 0)
        assert self.glCheckFramebufferStatus(FRAMEBUFFER) == 0x8CD5, "FP16 framebuffer is incomplete"
        self.glUseProgram(program)
        self.glViewport(0, 0, width, height)
        self.glUniform2f(self.glGetUniformLocation(program, b"uOutput"), width, height)
        for index, texture in enumerate(textures):
            assert texture != destination, "CNN framebuffer reads its own output"
            self.glActiveTexture(0x84C0 + index)
            self.glBindTexture(TEXTURE_2D, texture)
            self.glUniform1i(self.glGetUniformLocation(program, f"uBind{index}".encode()), index)
        self.glDrawArrays(0x0005, 0, 4)
        assert self.glGetError() == 0, "CNN draw generated an OpenGL error"

    def pixels(self, width, height):
        data = (F * (width * height * 4))()
        self.glReadPixels(0, 0, width, height, RGBA, FLOAT, data)
        return list(data)

    def delete_textures(self, textures):
        values = (U * len(textures))(*textures)
        self.glDeleteTextures(len(textures), values)


def fixtures(gl, header, hooks):
    # Each channel must reach exactly its upstream 2x2 subpixel position.
    source = gl.texture(1, 1, [0, 0.25, 0.5, 1])
    destination = gl.texture(2, 2)
    program = gl.program(builder.fragment_shader(header, hooks[-1]))
    gl.render(program, [source], destination, 2, 2)
    assert gl.pixels(2, 2)[::4] == [0, 0.5, 0.25, 1], "FSRCNNX subpixel phase/channel order changed"
    gl.glDeleteProgram(program)
    gl.delete_textures([source, destination])

    # Row order, BT.601 conversion in gamma-coded RGB, and replicated left edge.
    colors = [(1, 0, 0), (0, 1, 0), (0, 0, 1), (0.25, 0.5, 0.75)]
    source = gl.texture(2, 2, [component for rgb in colors for component in (*rgb, 1)])
    destination = gl.texture(2, 2)
    expected = [sum(c*w for c, w in zip(rgb, (0.299, 0.587, 0.114))) for rgb in colors]
    for offset, indices in (("0", [0, 1, 2, 3]), ("vec2(-1,0)", [0, 0, 2, 2])):
        hook = builder.Hook("fixture", ("LUMA",), "fixture", 1,
                            f"vec4 hook(){{return LUMA_texOff({offset});}}\n")
        program = gl.program(builder.fragment_shader(header, hook))
        gl.render(program, [source], destination, 2, 2)
        actual = gl.pixels(2, 2)[::4]
        assert max(abs(actual[i] - expected[index]) for i, index in enumerate(indices)) < 0.001
        gl.glDeleteProgram(program)
    gl.delete_textures([source, destination])


def main():
    builder.self_test()
    generated, _ = builder.generate()
    assert builder.DESTINATION.read_text(encoding="utf-8") == generated, "Generated CNN header is stale"
    gl = HeadlessGL()
    result = {"version": gl.glGetString(0x1F02).decode(), "renderer": gl.glGetString(0x1F01).decode(),
              "compiled_model_passes": 0, "fixtures": "phase / BT.601 / row order / edge clamp", "models": []}
    samples = [(value, value, value) for value in (0, 0.25, 0.5, 0.75, 1)]
    samples += [(1, 0, 0), (0, 1, 0), (0, 0, 1), (0.25, 0.5, 0.75)]
    for filename in builder.MODELS:
        header, hooks = builder.parse_model((builder.VENDOR / filename).read_text(encoding="utf-8"))
        fixtures(gl, header, hooks)
        schedule, slot_count, final_slot = builder.schedule(hooks)
        programs = [gl.program(builder.fragment_shader(header, hook)) for hook in hooks]
        result["compiled_model_passes"] += len(programs)
        width, height = 8, 8
        textures = [gl.texture(width * (2 if slot == final_slot else 1),
                               height * (2 if slot == final_slot else 1)) for slot in range(slot_count)]
        max_error = 0
        negative_feature = False
        for rgb in samples:
            expected = sum(c*w for c, w in zip(rgb, (0.299, 0.587, 0.114)))
            gl.upload(textures[0], width, height, [*rgb, 1] * (width * height))
            for program, (hook, bindings, output) in zip(programs, schedule):
                gl.render(program, [textures[slot] for slot in bindings], textures[output],
                          width * hook.scale, height * hook.scale)
                if output == 1:
                    negative_feature |= min(gl.pixels(width, height)) < -0.01
            values = gl.pixels(width * 2, height * 2)[::4]
            assert all(math.isfinite(value) for value in values), f"Non-finite CNN output: {filename}, {rgb}"
            max_error = max(max_error, max(abs(value - expected) for value in values))
        # This is a sanity bound on constant inputs, not a picture-quality claim.
        assert max_error < 0.12, f"Large constant-luma drift: {filename}: {max_error}"
        assert negative_feature, "Feature tensors were clamped to unsigned color values"
        result["models"].append({"model": filename, "constant_rgb_patches": len(samples),
                                 "max_constant_luma_error": max_error, "signed_features": True})
        for program in programs:
            gl.glDeleteProgram(program)
        gl.delete_textures(textures)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
