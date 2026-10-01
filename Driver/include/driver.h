#pragma once
#include "ntifs.h"
#include <wdf.h>

NTSTATUS DriverAddDevice(IN DRIVER_OBJECT* DriverObject, IN DEVICE_OBJECT* PhysicalDeviceObject);

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING theRegistryPath);

NTSTATUS DriverUnload(IN PDRIVER_OBJECT pDiverObject);