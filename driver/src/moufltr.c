#include "moufltr.h"

#pragma alloc_text(NONPAGE, MouseCallback)

#define LENIENCY_LIMIT		5	// How many times the SQ of the mouse coordinates can be the same
#define SNAPSHOT_INTERVAL	5	// The interval between snapshots
#define ACCELERATION_LIMIT  50
#define MAX_LATENCY         10'000 // NS

SnapshotManager g_SnapshotManager = { 0 };
MFReport g_MFReport = { 0 };

VOID MouseCallback(PDEVICE_OBJECT DeviceObject, PMOUSE_INPUT_DATA InputDataStart, PMOUSE_INPUT_DATA InputDataEnd, PULONG InputDataConsumed)
/*
https://learn.microsoft.com/en-us/previous-versions/ff542394(v=vs.85)/
*/
{
    PFDEVICE_EXTENSION ext = (PFDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	ULONG mouseMask = MOUSE_LEFT_BUTTON_DOWN; // maybe more?

    for (PMOUSE_INPUT_DATA p = InputDataStart; p < InputDataEnd; ++p)
    {
		if (p->ButtonFlags & mouseMask)
		{
			ReportMouseClick(p);
		}

		StorePacketData(p);

		MonitorLinearity();
		MonitorAcceleration();
    }

	// TODO: 
	// - Could do some mouse behavior checking? As is wether movement is too linear (robot-like)
	// - If there are no packets, but the game is playing there is a sign of cheating!


    (*(PSERVICE_CALLBACK_ROUTINE)ext->UpperConnectData.ClassService)(
        ext->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
    );
}

VOID ReportMouseClick(PMOUSE_INPUT_DATA p)
{
    LARGE_INTEGER CurrentTime;
	KeQuerySystemTimePrecise(&CurrentTime);

	MonitorPressInterval(g_DriverExtention->MouseData.Time, CurrentTime);

    g_DriverExtention->MouseData.ButtonFlags = p->ButtonFlags;
    g_DriverExtention->MouseData.Time = CurrentTime;
	DbgPrint("[AC] Hit a mouse callback!");
    return;
}

VOID MonitorPressInterval(LARGE_INTEGER pTime, LARGE_INTEGER cTime)
{
	LONGLONG diff = cTime.QuadPart - pTime.QuadPart;
	LONG intervaldiff = (LONG)(g_SnapshotManager.mousepressInterval - diff);

	if (intervaldiff < 0)
		intervaldiff *= -1;

	if (intervaldiff <= MAX_LATENCY)
	{
		g_SnapshotManager.mousepressCounter++;
	}

	if (g_SnapshotManager.mousepressCounter >= LENIENCY_LIMIT)
	{
		g_MFReport.flaggedMousePress = TRUE;
		g_SnapshotManager.mousepressCounter = 0;
	}
}

VOID HandleGameMouseClick()
{
	LARGE_INTEGER CurrentTime;

	// KeQuerySystemTime returns times in blocks which are not accurate enough..
	KeQuerySystemTimePrecise(&CurrentTime);

	LONGLONG diff = CurrentTime.QuadPart - g_DriverExtention->MouseData.Time.QuadPart;

	DbgPrint("NS difference: %I64d\n", diff);

	if (diff >= 1'000'000)
	{
		// TODO: increment flag count
	}
}

VOID StorePacketData(PMOUSE_INPUT_DATA p)
{
	g_SnapshotManager.pdx = g_SnapshotManager.dx;
	g_SnapshotManager.pdy = g_SnapshotManager.dy;

	if (p->Flags & MOUSE_MOVE_ABSOLUTE)
	{
		g_SnapshotManager.dx = p->LastX - g_SnapshotManager.prevX;
		g_SnapshotManager.dy = p->LastY - g_SnapshotManager.prevY;
	}
	if (p->Flags & MOUSE_MOVE_RELATIVE)
	{
		g_SnapshotManager.dx = p->LastX;
		g_SnapshotManager.dy = p->LastY;
	}

	g_SnapshotManager.prevX = g_SnapshotManager.lastX;
	g_SnapshotManager.prevY = g_SnapshotManager.lastY;

	g_SnapshotManager.lastX = p->LastX;
	g_SnapshotManager.lastY = p->LastY;
}

VOID MonitorLinearity()
/*
Just tests if to movement is linear (for now?)
*/
{
	LONG dx = g_SnapshotManager.dx;
	LONG dy = g_SnapshotManager.dy;
	LONG pdx = g_SnapshotManager.pdx;
	LONG pdy = g_SnapshotManager.pdy;

	if (dx == 0 && dy == 0)
	{
		g_SnapshotManager.movementCounter = 0;
		return ;
	}

	BOOLEAN sus = FALSE;

	if (dx == pdx && dy == pdy) 
	{
		g_SnapshotManager.movementCounter++;

		if (g_SnapshotManager.movementCounter >= LENIENCY_LIMIT)
			sus = TRUE;
	}
	else
	{
		g_SnapshotManager.movementCounter = 0;
	}

	if (sus)
		g_MFReport.flaggedLinearMovement = TRUE;
}

VOID MonitorAcceleration()
{
	LONG ax = g_SnapshotManager.dx - g_SnapshotManager.pdx;
	LONG ay = g_SnapshotManager.dy - g_SnapshotManager.pdy;

	if (ax >= ACCELERATION_LIMIT || ay >= ACCELERATION_LIMIT)
	{
		g_MFReport.flaggedAcceleration = TRUE;
	}
}

NTSTATUS PassIRP(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	if (IsACDevice(DeviceObject))
	{
		IoSkipCurrentIrpStackLocation(Irp);
		Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PFDEVICE_EXTENSION devExt = (PFDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	if (!devExt || !devExt->NextLowerDeviceObject)
	{
		IoSkipCurrentIrpStackLocation(Irp);

		Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		DbgPrintEx(DPFLTR_IHVDRIVER_ID,
			DPFLTR_INFO_LEVEL,
			"[AC] Device extention not initialized!\n");
		return STATUS_INVALID_DEVICE_REQUEST;
	}



	IoSkipCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	status = IoCallDriver(devExt->NextLowerDeviceObject, Irp);

	if (NT_SUCCESS(status))
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[AC] Passing IRP SUCCESS!\n");
	else
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[AC] Passing IRP FAILED!\n");

	return status;
}

NTSTATUS PassPowerIRP(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	PFDEVICE_EXTENSION devExt = (PFDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	if (!devExt || !devExt->NextLowerDeviceObject)
	{
		// Must notify the power manager before completing a power IRP!
		PoStartNextPowerIrp(Irp);

		Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		DbgPrintEx(DPFLTR_IHVDRIVER_ID,
			DPFLTR_INFO_LEVEL,
			"[AC] Device extention not initialized!\n");
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);

	return PoCallDriver(devExt->NextLowerDeviceObject, Irp);
}

BOOLEAN IsACDevice(DEVICE_OBJECT* DeviceObject)
/*
Is the incoming IRP for the filter driver or our deviceless driver?
TRUE: deviceless
FALSE: filter
*/
{
	if (DeviceObject == g_DriverExtention->DeviceObject)
		return TRUE;
	else
		return FALSE;
}

MFReport GetMFReport()
{
	return g_MFReport;
}