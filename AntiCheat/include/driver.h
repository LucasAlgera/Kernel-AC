#pragma once
#include "ntddk.h"
#include <wdf.h>

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING theRegistryPath);

NTSTATUS DriverUnload(IN PDRIVER_OBJECT pDiverObject);