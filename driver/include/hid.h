#pragma once
#include "common.h"

#pragma warning(push)
#pragma warning(disable: 4201)

#define MAX_FW_STRING 256
#define KEY_COUNT     2

VOID RegisterHUBCallbacks();

NTSTATUS EnumerateUSBHub();

typedef struct _FirmwareData
{
    union {
        WCHAR Fields[KEY_COUNT][MAX_FW_STRING];
        struct {
            WCHAR Manufacturer[MAX_FW_STRING];
            WCHAR HardwareIds[MAX_FW_STRING];
        };
    };
} FirmwareData, *PFirmwareData;

typedef struct _HIDResults
{
	BOOLEAN detection;
} HIDResults, *PHIDResults;

NTSTATUS RetrieveFirmware(_In_ PDEVICE_OBJECT pdo, _Out_ PFirmwareData fwdata);

VOID AnalyzeFirmware(_In_ PFirmwareData fwdata);

HIDResults GetHIDResults();