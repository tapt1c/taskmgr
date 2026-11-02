#ifndef TM95_VXD_ABI_H
#define TM95_VXD_ABI_H
/* Win9x ring-0 ABI, independent of SDK/DDK headers. */
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
#pragma pack(push,1)
struct DDB {
    DWORD next;
    WORD sdk,device;
    BYTE major,minor;
    WORD flags;
    char name[8];
    DWORD order;
    void (*control)(void);
    DWORD v86,pm,v86cs,pmcs,reference,services,service_count;
    DWORD win32_services,previous,size,reserved1,reserved2,reserved3;
};
#pragma pack(pop)
struct DIOC {
    DWORD internal1,vm,internal2,code;
    void *input;
    DWORD input_size;
    void *output;
    DWORD output_size;
    DWORD *returned;
    DWORD overlapped,device,process;
};
typedef char tm95_ddb_size_check[sizeof(struct DDB)==80?1:-1];
typedef char tm95_dioc_size_check[sizeof(struct DIOC)==48?1:-1];
#endif
