#pragma once
#include "common.h"

VOID RegisterHUBCallbacks();

NTSTATUS EnumerateUSBHub();

typedef struct _FirmwareData
{
	LONG a;
} FirmwareData, *PFirmwareData;

typedef struct _HIDResults
{
	BOOLEAN detection;
} HIDResults, *PHIDResults;

NTSTATUS RetrieveFirmware(_In_ PDEVICE_OBJECT pdo, _Out_ PFirmwareData fwdata);

VOID AnalyzeFirmware(_In_ PFirmwareData fwdata);

HIDResults GetHIDResults();