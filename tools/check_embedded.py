"""Check that RCDATA/214 contains the exact built LE VxD, not a placeholder."""
import struct
import sys
from pathlib import Path
exe, driver = map(Path, sys.argv[1:])
data = exe.read_bytes()
vxd = driver.read_bytes()
u16 = lambda p: struct.unpack_from('<H', data, p)[0]
u32 = lambda p: struct.unpack_from('<I', data, p)[0]
pe = u32(0x3c)
opt = pe+24
assert u16(opt) == 0x10b, 'Expected the shipping x86 PE32 build'
sections = pe+24+u16(pe+20)
def offset(rva):
    for i in range(u16(pe+6)):
        sec = sections+40*i
        virtual_size, start, size, filepos = struct.unpack_from('<IIII',data,sec+8)
        if start <= rva < start+max(size,virtual_size):
            return filepos+rva-start
    raise AssertionError('Invalid resource RVA')
root = offset(u32(opt+112))
def entry(directory, ident):
    for i in range(u16(directory+12)+u16(directory+14)):
        key, value = struct.unpack_from('<II', data, directory+16+i*8)
        if key == ident:
            return value
    raise AssertionError(f'Resource {ident} missing')
value = entry(root,10)  # RT_RCDATA
assert value & 0x80000000
value = entry(root+(value & 0x7fffffff),214)
assert value & 0x80000000
dirpos = root+(value & 0x7fffffff)
# First language leaf. There is exactly one copy of the language-neutral driver.
assert u16(dirpos+12)+u16(dirpos+14) == 1
leaf = u32(dirpos+20)
assert not leaf & 0x80000000
rva, length = struct.unpack_from('<II',data,root+leaf)
assert data[offset(rva):offset(rva)+length] == vxd, 'Embedded driver differs from the build'
assert vxd[:2] == b'MZ'
le = struct.unpack_from('<I',vxd,0x3c)[0]
assert vxd[le:le+2] == b'LE'
table,count = struct.unpack_from('<II',vxd,le+0x40)
for i in range(count):
    base,flags = struct.unpack_from('<II',vxd,le+table+24*i+4)
    assert base == 0 and flags & 4, 'VxD object is not normalized'
print(f'PASS: RCDATA/214 embeds exact LE VxD ({length} bytes); execution NOT tested')
