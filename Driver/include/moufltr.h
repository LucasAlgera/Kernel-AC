#pragma once
#include "common.h"
#include <ntddmou.h>

VOID MouseCallback(_In_    PDEVICE_OBJECT    DeviceObject,
				   _In_    PMOUSE_INPUT_DATA InputDataStart,
				   _In_    PMOUSE_INPUT_DATA InputDataEnd,
				   _Inout_ PULONG            InputDataConsumed);

NTSTATUS PassIRP(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS PassPowerIRP(DEVICE_OBJECT* DeviceObject, IRP* Irp);

BOOLEAN IsACDevice(DEVICE_OBJECT* DeviceObject);