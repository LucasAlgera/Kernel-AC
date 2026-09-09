#pragma once
#include "ntddk.h"

NTSTATUS DispatchDeviceControl(DEVICE_OBJECT* DeviceObject, IRP* Irp);
NTSTATUS CreateCloseHandler(DEVICE_OBJECT* DeviceObject, IRP* Irp);