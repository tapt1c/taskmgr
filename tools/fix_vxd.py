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
# This driver's DDB must be first, in the sole locked object. In particular,
# Watcom BSS must not precede it when state variables are added to the driver.
assert count == 1, 'Expected a single locked object for code, state and DDB'
assert struct.unpack_from('<I', data, le+table+8)[0] & 0x8000, 'Missing IOPL object flag'
page_data = struct.unpack_from('<I', data, le+0x80)[0]
expected = (sys.argv[2] if len(sys.argv)>2 else 'TM95STAT').encode('ascii')
assert len(expected)==8 and data[page_data+12:page_data+20] == expected, 'DDB is not first in the locked object'
assert struct.unpack_from('<I', data, page_data+64)[0] == 80, 'Invalid DDB size'
# WLINK treats an IOPL export as an OS/2 16-bit call gate. Ordinal 1 here is
# DDB data, not a procedure: retain the IOPL object but emit a direct 32-bit
# LE entry, as used by VxD DDB exports. Both layouts occupy five bytes.
entry = le + struct.unpack_from('<I', data, le+0x5c)[0]
entries, kind, obj = struct.unpack_from('<BBH', data, entry)
assert entries == 1 and obj == 1, 'Expected one DDB export from object 1'
assert data[entry+4] & 1, 'DDB ordinal 1 is not exported'
if kind == 2:
    offset, selector = struct.unpack_from('<HH', data, entry+5)
    assert offset == 0 and selector == 0, 'Unexpected DDB call-gate payload'
    data[entry+1] = 3
    struct.pack_into('<I', data, entry+5, offset)
else:
    assert kind == 3, 'Expected direct 32-bit DDB export'
assert struct.unpack_from('<I', data, entry+5)[0] == 0, 'DDB export is not at object start'
p.write_bytes(data)
print(f'LE VxD: {count} object(s), {len(data)} bytes; runtime UNTESTED')
