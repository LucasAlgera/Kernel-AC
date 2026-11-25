#pragma once
#include "ntifs.h"

typedef enum 
{
    VERIFY_SNAPSHOT_HASH = 0,
    WALK_PROCESS_LIST = (1U << 0),
    SCAN_FOR_MANUALLY_MAPPED_CODE = (1U << 1),
    GET_CURRENT_STATUS = (1U << 2),
    CHECK_HWBP = (1U << 3),
    ENUMERATE_USB_HUB = (1U << 4)
} RuntimeValidators;

NTSTATUS DispatchDeviceControl(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS InternalDispatchDeviceControl(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS CreateCloseHandler(DEVICE_OBJECT* DeviceObject, IRP* Irp);

// Non-IO related functions
NTSTATUS RuntimeValidations(DEVICE_OBJECT* DeviceObject, IRP* Irp);