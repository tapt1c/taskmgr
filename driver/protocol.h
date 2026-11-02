#ifndef TM95_VXD_PROTOCOL_H
#define TM95_VXD_PROTOCOL_H
#ifndef TM95_RING0
#include <windows.h>
#endif
#define TM95_PROTOCOL_VERSION 3UL
#define TM95_IOCTL_QUERY 1UL
#define TM95_IOCTL_CAD 2UL
#define TM95_CAP_CAD 4UL
#define TM95_CAP_UPTIME 1UL
#define TM95_CAP_PAGE_COUNTS 2UL
/* Only advertise implemented capabilities; per-process metrics remain separate. */
typedef struct {
    DWORD size, version, capabilities;
    DWORD uptime_ms, free_pages, system_pages;
    DWORD cad_events, cad_armed;
} TM95_VXD_SAMPLE;
typedef char tm95_sample_size_must_be_32[(sizeof(TM95_VXD_SAMPLE)==32)?1:-1];
typedef struct {
    DWORD size,version,enable;
} TM95_CAD_REQUEST;
/* A separate, inert device used only to diagnose dynamic loading failures. */
#define TM95_PROBE_MAGIC 0x39354d54UL
typedef struct { DWORD size,magic; } TM95_PROBE_SAMPLE;
#endif
