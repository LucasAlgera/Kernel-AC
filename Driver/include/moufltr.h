#pragma once
#include "common.h"
#include <ntddmou.h>

VOID MouseCallback(PDEVICE_OBJECT    DeviceObject,
				   PMOUSE_INPUT_DATA InputDataStart,
				   PMOUSE_INPUT_DATA InputDataEnd,
				   PULONG            InputDataConsumed);

VOID ReportMouseClick(PMOUSE_INPUT_DATA p);

BOOLEAN IsMovementSuspicous(PMOUSE_INPUT_DATA p);

NTSTATUS PassIRP(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS PassPowerIRP(DEVICE_OBJECT* DeviceObject, IRP* Irp);

BOOLEAN IsACDevice(DEVICE_OBJECT* DeviceObject);

typedef struct _SnapshotManager
{
	LONG prevX, prevY;
	LONG dx, dy;
	LONG counter;
	LONG packetCount;
} SnapshotManager, *PSnapshotManager;
