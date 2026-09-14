#pragma once
#include "ntifs.h"

// Import undocumented exported functions from ntoskrnl.exe:
EXTERN_C NTSYSAPI UCHAR* PsGetProcessImageFileName(		__in PEPROCESS Process);
EXTERN_C NTSYSAPI NTSTATUS PsLookupProcessByProcessId(	__in HANDLE ProcessId,
														__out PEPROCESS* Process);
EXTERN_C NTSYSAPI PVOID PsGetProcessSectionBaseAddress(	__in PEPROCESS Process);

typedef unsigned long DWORD;
typedef unsigned char BYTE;
typedef BYTE* PBYTE;

#ifndef _DRIVER_SETTINGS_DEFINED
#define _DRIVER_SETTINGS_DEFINED
typedef struct _DRIVER_SETTINGS
{
	DWORD PID;						// PiD of the game
	unsigned char hashes[10][32];	// hashes of sections of code

} DRIVER_SETTINGS, * PDRIVER_SETTINGS;
#endif

extern PDRIVER_SETTINGS g_DriverExtention;

#define WHITELIST_SIZE 5
#define MAX_PROCESS_NAME_LENGTH 256

extern CHAR g_Whitelist[WHITELIST_SIZE][MAX_PROCESS_NAME_LENGTH];

#define PROCESS_CREATE_PROCESS            0x0080
#define PROCESS_TERMINATE                 0x0001
#define PROCESS_CREATE_THREAD             0x0002
#define PROCESS_QUERY_INFORMATION         0x0400
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define PROCESS_SET_INFORMATION           0x0200
#define PROCESS_SET_QUOTA                 0x0100
#define PROCESS_SUSPEND_RESUME            0x0800
#define PROCESS_VM_OPERATION              0x0008
#define PROCESS_VM_READ                   0x0010
#define PROCESS_VM_WRITE                  0x0020