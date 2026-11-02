"""Check that a driver RCDATA contains the exact built LE VxD, not a placeholder."""
import struct
import sys
from pathlib import Path
exe, driver = map(Path, sys.argv[1:3])
resource_id = int(sys.argv[3]) if len(sys.argv)>3 else 214
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
value = entry(root+(value & 0x7fffffff),resource_id)
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
entry = le+struct.unpack_from('<I',vxd,le+0x5c)[0]
assert struct.unpack_from('<BBH',vxd,entry) == (1,3,1), 'DDB needs a direct 32-bit ordinal 1 export'
assert struct.unpack_from('<I',vxd,entry+5)[0] == 0, 'DDB export must start object 1'
for i in range(count):
    base,flags = struct.unpack_from('<II',vxd,le+table+24*i+4)
    assert base == 0 and flags & 4 and flags & 0x8000, 'VxD object lacks flat/executable/IOPL attributes'
print(f'PASS: RCDATA/{resource_id} embeds exact LE VxD ({length} bytes); execution NOT tested')
