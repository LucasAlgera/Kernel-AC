#pragma once
#include "common.h"
#pragma comment(lib, "Cng.lib") // KM imports for bcrypt.lib

NTSTATUS ComputeSHA256(
    _In_reads_bytes_(inputlen) const unsigned char* input,
    _In_ ULONG inputlen,
    _Out_writes_bytes_(32) unsigned char* hash
);

NTSTATUS GetTextSectionFromMonitoredProcess(PUCHAR* text, uintptr_t offset);

NTSTATUS TakeHashSnapshot(DEVICE_OBJECT* DeviceObject, IRP* Irp);
