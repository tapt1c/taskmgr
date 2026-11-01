"""Normalize Watcom LE objects for Win9x; same format requirement as
https://github.com/JHRobotics/fixlink (-vxd32), without importing its source.
This validates the file format, not the kernel ABI or runtime behavior.
"""
from pathlib import Path
import struct
import sys
p = Path(sys.argv[1])
data = bytearray(p.read_bytes())
assert data[:2] == b'MZ'
le = struct.unpack_from('<I', data, 0x3c)[0]
assert data[le:le+2] == b'LE', 'Expected a real LE VxD, not a renamed PE'
table, count = struct.unpack_from('<II', data, le+0x40)
assert 0 < count < 32
for i in range(count):
    pos = le+table+i*24
    assert pos+24 <= len(data)
    flags = struct.unpack_from('<I', data, pos+8)[0]
    struct.pack_into('<II', data, pos+4, 0, flags | 4)
p.write_bytes(data)
print(f'LE VxD: {count} object(s), {len(data)} bytes; runtime UNTESTED')
