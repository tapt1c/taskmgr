#ifndef TM95_VXD_PROTOCOL_H
#define TM95_VXD_PROTOCOL_H
/* DRAFT ONLY. Shared wire layout for a future Windows 95 DDK VxD.
   No driver is built or loaded by the current executable.
   DWORD fields only: no pointers, compiler-sized enums, or 64-bit alignment. */
#include <windows.h>
#define TM95_PROTOCOL_VERSION 1UL
#define TM95_IOCTL_QUERY 1UL
#define TM95_CAP_CPU_TOTAL 1UL
/* A future DeviceIoControl request uses no input for QUERY. Output must be
   validated by size, version and capabilities before interpreting counters. */
typedef struct {
    DWORD size;
    DWORD version;
    DWORD capabilities;
    DWORD sample_sequence;
    DWORD total_ticks;
    DWORD idle_ticks;
} TM95_VXD_SAMPLE;
typedef char tm95_sample_size_must_be_24[(sizeof(TM95_VXD_SAMPLE) == 24) ? 1 : -1];
#endif
