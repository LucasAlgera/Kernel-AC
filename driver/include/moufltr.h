#pragma once
#include "common.h"
#include <ntddmou.h>

VOID MouseCallback(PDEVICE_OBJECT    DeviceObject,
				   PMOUSE_INPUT_DATA InputDataStart,
				   PMOUSE_INPUT_DATA InputDataEnd,
				   PULONG            InputDataConsumed);

// Game-related functions
VOID ReportMouseClick(PMOUSE_INPUT_DATA p);

VOID HandleGameMouseClick();


// Behavior-related functions
VOID StorePacketData(PMOUSE_INPUT_DATA p);

VOID MonitorLinearity();

VOID MonitorAcceleration();

VOID MonitorPressInterval(LARGE_INTEGER pTime, LARGE_INTEGER cTime);

// Other
NTSTATUS PassIRP(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS PassPowerIRP(DEVICE_OBJECT* DeviceObject, IRP* Irp);

BOOLEAN IsACDevice(DEVICE_OBJECT* DeviceObject);

MFReport GetMFReport();

typedef struct _SnapshotManager
{
	// Current packet info
	LONG lastX, lastY;
	LONG dx, dy;

	// Previous packet info
	LONG prevX, prevY;
	LONG pdx, pdy;

	LONG movementCounter;
	LONG packetCount;
	
	// for mousepress monitoring
	LONGLONG mousepressInterval;
	LONG mousepressCounter;

} SnapshotManager, *PSnapshotManager;

typedef struct _MFReport
{
	BOOLEAN flaggedAcceleration;
	BOOLEAN flaggedLinearMovement;
	BOOLEAN flaggedMousePress;
	// ...
} MFReport, *PMFReport;