"""Pack this project's single-object LE VxDs in Win9x disk layout.

WLINK leaves num_preload as zero (loadflat.c: NYI), uses 4 KiB LE pages,
and emits an OS/2 autodata object. Use 512-byte disk pages, a page-aligned
payload and explicit preload metadata, as in the WinIo 2.0 Win9x VxD.
This is not a general-purpose LE converter or a Win95 runtime test.
"""
from pathlib import Path
import struct
import sys

HEADER_SIZE = 0xc4
DISK_PAGE = 512
DATA_ALIGN = 4096


def pack(source, identity):
    u32 = lambda off: struct.unpack_from('<I', source, off)[0]
    assert source[:2] == b'MZ'
    le = u32(0x3c)
    assert source[le:le+2] == b'LE', 'Expected LE, not PE'
    field = lambda off: u32(le+off)
    assert field(0x44) == 1, 'Only the project single locked object is supported'
    assert field(0x10) == 0x38000, 'Expected a dynamic Win9x VxD'
    assert not any(field(off) for off in (0x4c, 0x54, 0x60, 0x64, 0x74, 0x7c, 0x98, 0x9c, 0xb8, 0xbc)), 'Unsupported LE resources, imports or debug data'
    old_page, old_pages = field(0x28), field(0x14)
    assert old_page in (512, 4096) and 0 < old_pages < 32768
    obj = le+field(0x40)
    virtual_size, _, flags, first_page, page_count, _ = struct.unpack_from('<6I', source, obj)
    assert 80 <= virtual_size < 16*1024*1024 and flags & 0x2000
    assert first_page == 1 and page_count == old_pages
    page_map = le+field(0x48)
    for page in range(old_pages):
        assert source[page_map+4*page:page_map+4*page+4] == (page+1).to_bytes(3, 'big')+b'\0', 'Nonlinear or iterated LE page'
    size = (old_pages-1)*old_page+(field(0x2c) or old_page)
    assert 0 < size <= virtual_size
    payload = source[field(0x80):field(0x80)+size]
    assert len(payload) == size and len(identity) == 8
    assert payload[12:20] == identity, 'DDB is not first in the object'
    assert struct.unpack_from('<I', payload, 64)[0] == 80, 'Bad DDB size'
    entry = le+field(0x5c)
    count, kind, entry_obj = struct.unpack_from('<BBH', source, entry)
    assert (count, entry_obj) == (1, 1) and kind in (2, 3)
    assert source[entry+4] & 1 and u32(entry+5) == 0, 'Expected sole DDB export at object start'

    # Read internal 32-bit pointer fixups. WLINK may repeat a fixup on both
    # sides of a disk-page boundary. Deduplicate before changing page size.
    old_fixpages = le+field(0x68)
    old_records = le+field(0x6c)
    fixups = {}
    for page in range(old_pages):
        begin, end = struct.unpack_from('<II', source, old_fixpages+page*4)
        pos, limit = old_records+begin, old_records+end
        assert pos <= limit <= len(source)
        while pos < limit:
            stype, target_flags = struct.unpack_from('<BB', source, pos)
            pos += 2
            assert stype in (7, 0x27) and not target_flags & ~0x50, 'Unsupported LE fixup'
            if stype & 0x20:
                source_count = source[pos]
                pos += 1
                sources = None
            else:
                sources = [struct.unpack_from('<h', source, pos)[0]]
                pos += 2
            obj_fmt = '<H' if target_flags & 0x40 else '<B'
            target_obj = struct.unpack_from(obj_fmt, source, pos)[0]
            pos += struct.calcsize(obj_fmt)
            off_fmt = '<I' if target_flags & 0x10 else '<H'
            target = struct.unpack_from(off_fmt, source, pos)[0]
            pos += struct.calcsize(off_fmt)
            assert target_obj == 1 and target <= virtual_size
            if sources is None:
                sources = struct.unpack_from('<'+'h'*source_count, source, pos)
                pos += source_count*2
            for offset in sources:
                absolute = page*old_page+offset
                assert 0 <= absolute <= size-4
                assert absolute not in fixups or fixups[absolute] == target
                fixups[absolute] = target
        assert pos == limit
    assert fixups.get(24) == struct.unpack_from('<I', payload, 24)[0], 'Control pointer lacks its LE fixup'

    pages = (size+DISK_PAGE-1)//DISK_PAGE
    per_page = [bytearray() for _ in range(pages)]
    for position, target in sorted(fixups.items()):
        # A pointer spanning sectors needs a record in both sectors; the second
        # record has a negative source offset. Do not merely change page_size.
        for page in range(position//DISK_PAGE, (position+3)//DISK_PAGE+1):
            if target <= 0xffff:
                record = struct.pack('<BBhBH', 7, 0, position-page*DISK_PAGE, 1, target)
            else:
                record = struct.pack('<BBhBI', 7, 0x10, position-page*DISK_PAGE, 1, target)
            per_page[page].extend(record)
    offsets = [0]
    for records in per_page:
        offsets.append(offsets[-1]+len(records))

    header = bytearray(source[le:le+HEADER_SIZE])
    body = bytearray()
    def add(blob):
        offset = HEADER_SIZE+len(body)
        body.extend(blob)
        return offset
    def set_field(off, value):
        struct.pack_into('<I', header, off, value)
    objoff = add(struct.pack('<6I', virtual_size, 0, 0x2047, 1, pages, int.from_bytes(b'LCOD', 'little')))
    mapoff = add(b''.join((i+1).to_bytes(3, 'big')+b'\0' for i in range(pages)))
    nameoff = add(bytes([8])+identity+b'\0\0\0')
    entryoff = add(struct.pack('<BBHBI', 1, 3, 1, 3, 0)+b'\0')
    loader_size = HEADER_SIZE+len(body)-objoff
    fixpageoff = add(struct.pack('<'+'I'*len(offsets), *offsets))
    fixrecoff = add(b''.join(per_page))
    importoff = HEADER_SIZE+len(body)
    dataoff = (le+HEADER_SIZE+len(body)+DATA_ALIGN-1)//DATA_ALIGN*DATA_ALIGN
    nonres_size = field(0x8c)
    nonres = source[field(0x88):field(0x88)+nonres_size] if nonres_size else b''
    assert len(nonres) == nonres_size
    for off, value in {
        0x14: pages, 0x28: DISK_PAGE, 0x2c: size % DISK_PAGE or DISK_PAGE,
        0x30: importoff-fixpageoff, 0x34: 0, 0x38: loader_size, 0x3c: 0,
        0x40: objoff, 0x44: 1, 0x48: mapoff, 0x50: 0, 0x58: nameoff,
        0x5c: entryoff, 0x68: fixpageoff, 0x6c: fixrecoff, 0x70: importoff,
        0x78: importoff, 0x80: dataoff, 0x84: pages,
        0x88: dataoff+size if nonres else 0, 0x8c: nonres_size, 0x90: 0,
        0x94: 0, 0xa0: 0, 0xa4: 0,
    }.items():
        set_field(off, value)
    result = bytearray(source[:le])+header+body
    result.extend(b'\0'*(dataoff-len(result)))
    result.extend(payload)
    result.extend(nonres)
    return result, pages


if __name__ == '__main__':
    path = Path(sys.argv[1])
    identity = (sys.argv[2] if len(sys.argv)>2 else 'TM95STAT').encode('ascii')
    result, pages = pack(path.read_bytes(), identity)
    path.write_bytes(result)
    print(f'LE VxD: {pages} preloaded 512-byte disk page(s), {len(result)} bytes; runtime UNTESTED')
