#ifndef TM95_VXD_PROTOCOL_H
#define TM95_VXD_PROTOCOL_H
#ifndef TM95_RING0
#include <windows.h>
#endif
#define TM95_PROTOCOL_VERSION 2UL
#define TM95_IOCTL_QUERY 1UL
#define TM95_CAP_UPTIME 1UL
#define TM95_CAP_PAGE_COUNTS 2UL
/* Read-only VMM counters. No CPU/per-process memory capability is claimed. */
typedef struct {
    DWORD size, version, capabilities;
    DWORD uptime_ms, free_pages, system_pages;
} TM95_VXD_SAMPLE;
typedef char tm95_sample_size_must_be_24[(sizeof(TM95_VXD_SAMPLE)==24)?1:-1];
#endif
