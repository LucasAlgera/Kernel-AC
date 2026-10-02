#pragma once
#include "ntifs.h"

NTSTATUS DispatchDeviceControl(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS InternalDispatchDeviceControl(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS CreateCloseHandler(DEVICE_OBJECT* DeviceObject, IRP* Irp);


//Non I/O related functions
NTSTATUS HandleProcessLaunch(IRP* Irp);

NTSTATUS WalkProcessList(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS ScanManuallyMappedCode(DEVICE_OBJECT* DeviceObject, IRP* Irp);