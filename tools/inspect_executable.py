"""Export original LE sections for read-only disassembly and resource research."""
import pathlib
import struct
import sys


def le_image(path):
    d = pathlib.Path(path).read_bytes()
    for mz in range(len(d) - 64):
        if d[mz:mz+2] != b'MZ':
            continue
        le = mz + struct.unpack_from('<I', d, mz + 60)[0]
        if d[le:le+4] == b'LE\0\0':
            break
    h = d[le:]
    rd = lambda o: struct.unpack_from('<I', h, o)[0]
    memory = bytearray(8 << 20)
    objects = []
    pages = {}
    for k in range(rd(68)):
        size, base, flags, first, count, _ = struct.unpack_from('<6I', h, rd(64) + k * 24)
        objects.append((base, size, flags))
        for p in range(count):
            pg = first + p
            pages[pg] = base + p * rd(40)
            q = rd(72) + (pg - 1) * 4
            num = int.from_bytes(h[q:q+3], 'big')
            n = rd(40) if pg < rd(20) else rd(44)
            src = mz + rd(128) + (num - 1) * rd(40)
            memory[base + p * rd(40):base + p * rd(40) + n] = d[src:src+n]
    for pg in range(1, rd(20) + 1):
        a = rd(rd(104) + (pg - 1) * 4) + rd(108)
        end = rd(rd(104) + pg * 4) + rd(108)
        while a < end:
            src, flags = h[a:a+2]
            a += 2
            if src & 32:
                count = h[a]
                a += 1
                single = 0
            else:
                count = 1
                single = struct.unpack_from('<h', h, a)[0]
                a += 2
            if flags & 3:
                raise ValueError('unsupported external fixup')
            n = 2 if flags & 64 else 1
            obj = int.from_bytes(h[a:a+n], 'little')
            a += n
            off = 0
            kind = src & 15
            if kind != 2:
                n = 4 if flags & 16 else 2
                off = int.from_bytes(h[a:a+n], 'little')
                a += n
            target = objects[obj-1][0] + off
            for j in range(count):
                offset = struct.unpack_from('<h', h, a + j*2)[0] if src & 32 else single
                va = pages[pg] + offset
                if kind in (7, 8):
                    struct.pack_into('<I', memory, va, (target if kind == 7 else target - va - 4) & 0xffffffff)
            if src & 32:
                a += count * 2
    return memory, objects


if __name__ == '__main__':
    memory, objects = le_image(sys.argv[1])
    dest = pathlib.Path(sys.argv[2])
    dest.mkdir(parents=True, exist_ok=True)
    (dest / 'memory.bin').write_bytes(memory)
    base, size, _ = objects[0]
    # Minimal ELF32 with one executable section; llvm-objdump can disassemble it.
    code = memory[base:base+size]
    names = b'\0.text\0.shstrtab\0'
    data = bytearray(52) + code + names
    shoff = len(data)
    data += bytes(40)
    data += struct.pack('<10I', 1, 1, 6, base, 52, len(code), 0, 0, 16, 0)
    data += struct.pack('<10I', 7, 3, 0, 0, 52 + len(code), len(names), 0, 0, 1, 0)
    data[:52] = struct.pack('<16sHHIIIIIHHHHHH', b'\x7fELF\x01\x01\x01'+bytes(9), 2, 3, 1, base, 0, shoff, 0, 52, 0, 0, 40, 3, 2)
    (dest / 'original.elf').write_bytes(data)
    print(objects)
