"""Private execution fixture for the locally owned German DOS executable.

No executable bytes, game assets or memory snapshots are distributed. Recreated
geometry/lookup tables are generated from their mathematical definitions.
"""
from pathlib import Path
import math
import struct
import sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from inspect_executable import le_image


def original_memory(game):
    memory, _ = le_image(game / 'M.EXE')
    assert memory[0x33d95] == 0x55 and struct.unpack_from('<i', memory, 0xb3a3c)[0] == 6144, 'German GOG executable required'
    trig = (game / 'DATA640/TABLEAU.COS').read_bytes()
    cosine = struct.unpack('<1280i', trig)
    assert all(cosine[i] == round(math.cos(i * math.tau / 1024) * 32768) for i in range(1280))
    memory[0x2a423c:0x2a423c + len(trig)] = trig
    for angle in range(1024):
        c, s = cosine[angle], cosine[angle + 256]
        struct.pack_into('<4i', memory, 0xa8180 + angle * 16,
                         (2500*c + 5800*s) >> 15, (5800*c - 2500*s) >> 15,
                         (-2500*c + 5800*s) >> 15, (5800*c + 2500*s) >> 15)
    for z in range(256):
        for x in range(256):
            xx = 2 * (x if x < 128 else x - 256) + 1
            zz = 2 * (z if z < 128 else z - 256) + 1
            memory[0xb5b08 + z*256 + x] = (math.isqrt(xx*xx + zz*zz) + 1) // 2
    struct.pack_into('<5i', memory, 0xac180, 0, 50, 974, 562, 462)
    return bytes(memory), cosine


class OriginalMachine:
    def __init__(self, memory):
        self.machine = Uc(UC_ARCH_X86, UC_MODE_32)
        self.machine.mem_map(0, 32 << 20)
        self.machine.mem_write(0, memory)

    def put(self, address, value):
        self.machine.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def get(self, address):
        return struct.unpack('<i', self.machine.mem_read(address, 4))[0]

    def call(self, address, **registers):
        from unicorn import x86_const
        for name, value in registers.items():
            self.machine.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()), value)
        self.machine.reg_write(UC_X86_REG_ESP, 0x1f00000)
        self.put(0x1f00000, 0x1e00000)
        self.machine.emu_start(address, 0x1e00000, count=5000000)
        if self.machine.reg_read(UC_X86_REG_EIP) != 0x1e00000:
            raise RuntimeError(f'Original routine {address:x} did not return')


def wsl_path(path):
    path = path.resolve()
    return '/mnt/' + path.drive[0].lower() + path.as_posix()[2:]
