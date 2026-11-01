#ifndef TM95_PROCESS_POLICY_H
#define TM95_PROCESS_POLICY_H
/* Exact, case-insensitive basename matching, independent of the host OS. */
int tm_process_name_protected(const char *path);
#endif
