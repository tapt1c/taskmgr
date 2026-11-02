"""Exercise the actual LE driver in x86 with controlled VMM services.
Requires Unicorn; does NOT execute Windows 95 or validate its VMM implementation.
"""
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_INTR
from unicorn.x86_const import *

base, stack, request, sentinel = 0x100000, 0x200000, 0x300000, 0x400000
u32 = lambda buf,pos: struct.unpack_from('<I',buf,pos)[0]
def image(path,identity):
    raw=Path(path).read_bytes()
    le=u32(raw,0x3c)
    assert raw[le:le+2]==b'LE'
    assert u32(raw,le+0x44)==1 and u32(raw,le+0x14)==1, 'Only a single locked page is supported by this test'
    obj=le+u32(raw,le+0x40)
    size=u32(raw,obj)
    start=u32(raw,le+0x80)
    code=bytearray(raw[start:start+size])
    assert code[12:20]==identity
    assert u32(raw,obj+8)&0x8000
    entry=le+u32(raw,le+0x5c)
    assert struct.unpack_from('<BBH',raw,entry)==(1,3,1)
    assert u32(raw,entry+5)==0
    records=le+u32(raw,le+0x6c)
    page_table=le+u32(raw,le+0x68)
    a,b=struct.unpack_from('<II',raw,page_table)
    for pos in range(records+a,records+b,7):
        source,flags,offset,target,value=struct.unpack_from('<BBhBH',raw,pos)
        assert (source,flags,target)==(7,0,1), 'Unexpected LE relocation'
        struct.pack_into('<I',code,offset,base+value)
    return code
code=image('build/tm95stat.vxd',b'TM95STAT')
uc=Uc(UC_ARCH_X86,UC_MODE_32)
for address in (base,stack,request,sentinel): uc.mem_map(address,0x10000)
uc.mem_write(base,bytes(code));uc.mem_write(sentinel+0x100,b'\xf8\xc3')
control=u32(code,24)
now=100
hook=0
hook_fail=unhook_fail=False
services=[]
def carry(value):
    flags=uc.reg_read(UC_X86_REG_EFLAGS)
    uc.reg_write(UC_X86_REG_EFLAGS,(flags|1) if value else (flags&~1))
def interrupt(machine,number,unused):
    global hook
    assert number==0x20
    ip=machine.reg_read(UC_X86_REG_EIP)
    service=u32(machine.mem_read(ip,4),0)
    machine.reg_write(UC_X86_REG_EIP,ip+4)
    services.append(service)
    if service==0x1003f: machine.reg_write(UC_X86_REG_EAX,now)
    elif service in (0x1005a,0x1005b): machine.reg_write(UC_X86_REG_EAX,1024 if service==0x1005a else 8192)
    elif service==0x10090:
        assert machine.reg_read(UC_X86_REG_EAX)==0xd0012
        hook=machine.reg_read(UC_X86_REG_ESI)
        # Unhook_Device_Service needs the standard chain header.
        assert bytes(machine.mem_read(hook,4))==b'\xeb\x06\xff\x25'
        machine.reg_write(UC_X86_REG_ESI,sentinel+0x100);carry(hook_fail)
    elif service==0x1011c:
        assert machine.reg_read(UC_X86_REG_EAX)==0xd0012
        assert machine.reg_read(UC_X86_REG_ESI)==hook
        carry(unhook_fail)
    else: raise AssertionError(f'Unexpected VMM service {service:x}')
uc.hook_add(UC_HOOK_INTR,interrupt)
def run(address,**registers):
    initial={UC_X86_REG_EAX:0x13579bdf,UC_X86_REG_EBX:0x2468ace0,UC_X86_REG_ECX:0x12345678,
             UC_X86_REG_EDX:0x11223344,UC_X86_REG_ESI:0x55667788,UC_X86_REG_EDI:0x76543210,UC_X86_REG_EBP:0x778899aa}
    for name,value in registers.items(): initial[globals()['UC_X86_REG_'+name]]=value
    for register,value in initial.items():uc.reg_write(register,value)
    uc.reg_write(UC_X86_REG_ESP,stack+0x8000);uc.reg_write(UC_X86_REG_EFLAGS,2)
    uc.mem_write(stack+0x8000,struct.pack('<I',sentinel))
    uc.emu_start(address,sentinel,timeout=1000000,count=100000)
    assert uc.reg_read(UC_X86_REG_EIP)==sentinel, 'Driver did not return'
    for register,value in initial.items():
        if register not in (UC_X86_REG_EAX,UC_X86_REG_ECX):assert uc.reg_read(register)==value, 'Clobbered register'
    return uc.reg_read(UC_X86_REG_EAX),uc.reg_read(UC_X86_REG_EFLAGS)&1

def ioctl(code,inp=b'',out=32,device=17):
    uc.mem_write(request+0x100,inp or b'\0')
    uc.mem_write(request+0x200,b'\0'*32);uc.mem_write(request+0x300,struct.pack('<I',999))
    uc.mem_write(request,struct.pack('<12I',0,0,0,code,request+0x100 if inp else 0,len(inp),request+0x200 if out else 0,out,request+0x300,0,device,0))
    result,_=run(control,EAX=0x23,ESI=request)
    length=u32(uc.mem_read(request+0x300,4),0)
    sample=struct.unpack('<8I',uc.mem_read(request+0x200,32))
    return result,length,sample

def key(scan):
    result,_=run(hook,ECX=0x12345600|scan)
    assert result==0x13579bdf
    return uc.reg_read(UC_X86_REG_ECX)&255

enable=struct.pack('<3I',12,3,1)
assert ioctl(0)[0]==0
assert ioctl(1)==(0,32,(32,3,7,100,1024,8192,0,0))
assert ioctl(1,out=31)[0:2]==(87,0)
assert ioctl(99)[0:2]==(50,0)
assert ioctl(2,struct.pack('<3I',12,99,1),out=0)[0]==87
assert ioctl(2,enable,out=0)[0]==0
assert ioctl(2,enable,out=0,device=18)[0]==170
assert key(0x1d)==0x1d and key(0x38)==0x38 and key(0xe0)==0xe0
assert key(0x53)==0xd3, 'First CAD must become a Delete release, preserving E0 pairing'
assert ioctl(1)[2][6:]==(1,1)
assert key(0xe0)==0xe0 and key(0xd3)==0xd3
now=200
assert key(0xe0)==0xe0 and key(0x53)==0x53, 'Second deliberate CAD must reach native handler'
key(0xe0);key(0xd3)
now=4201
assert key(0x53)==0x53, 'Hung user-mode listener must not trap CAD'
key(0xd3)
now=4500;ioctl(1)
assert key(0x53)==0xd3
assert ioctl(1)[2][6:]==(2,1)
key(0xd3)
assert ioctl(0xffffffff,out=0)[0]==0
assert ioctl(1)[2][7]==0
hook_fail=True
assert ioctl(2,enable,out=0)[0]==50
hook_fail=False
assert ioctl(2,enable,out=0)[0]==0
unhook_fail=True
assert ioctl(0xffffffff,out=0)[0]==170
assert run(control,EAX=0x1c)[1]==1, 'Refuse unload while another hook keeps our code in its chain'
unhook_fail=False
assert run(control,EAX=0x1c)[1]==0
assert ioctl(1)[2][7]==0
print('PASS: actual LE x86 code, DIOC ABI, buffer sizes, hook chaining, CAD, heartbeat, ownership and unload')
print('VMM is emulated; real Windows 95/VKD execution is NOT tested')

# The diagnostic device must load/query/unload without a single VMM service.
code=image('build/tm95test.vxd',b'TM95TEST')
uc=Uc(UC_ARCH_X86,UC_MODE_32)
for address in (base,stack,request,sentinel): uc.mem_map(address,0x10000)
uc.hook_add(UC_HOOK_INTR,interrupt)
uc.mem_write(base,bytes(code));control=u32(code,24);services.clear()
assert run(control,EAX=0x1b)[1]==0
assert ioctl(0)[0]==0
assert ioctl(1,out=8)[0:2]==(0,8)
assert ioctl(1,out=8)[2][0:2]==(8,0x39354d54)
assert ioctl(1,out=7)[0:2]==(87,0)
assert ioctl(99)[0]==50
assert ioctl(0xffffffff,out=0)[0]==0
assert run(control,EAX=0x1c)[1]==0
assert not services
print('PASS: inert probe LE loads/answers/unloads in x86 without VMM calls')
