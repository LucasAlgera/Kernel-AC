#include "moufltr.h"

#pragma alloc_text(NONPAGE, MouseCallback)

#define LENIENCY_LIMIT		5	// How many times the SQ of the mouse coordinates can be the same
#define SNAPSHOT_INTERVAL	5	// The interval between snapshots

SnapshotManager g_SnapshotManager = { 0 };

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

		if (g_SnapshotManager.packetCount >= SNAPSHOT_INTERVAL)
		{
			g_SnapshotManager.packetCount = 0;

			if (IsMovementSuspicous(p))
				DbgPrint("[AC] LINEAR MOVEMENT!");
		}

		g_SnapshotManager.packetCount++;
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
    KeQuerySystemTime(&CurrentTime);

    g_DriverExtention->MouseData.ButtonFlags = p->ButtonFlags;
    g_DriverExtention->MouseData.Time = CurrentTime;
	DbgPrint("[AC] Hit a mouse callback!");
    return;
}

BOOLEAN IsMovementSuspicous(PMOUSE_INPUT_DATA p)
/*
Just tests if to movement is linear (for now?)
*/
{
	if (p->LastX == 0 && p->LastY == 0)
	{
		g_SnapshotManager.counter = 0;
		return FALSE;
	}

	LONG dy = p->LastY - g_SnapshotManager.prevY;
	LONG dx = p->LastX - g_SnapshotManager.prevX;


	BOOLEAN sus = FALSE;

	if (dx == g_SnapshotManager.dx && dy == g_SnapshotManager.dy)
	{
		g_SnapshotManager.counter++;
		if (g_SnapshotManager.counter >= LENIENCY_LIMIT)
			sus = TRUE;
	}
	else
	{
		g_SnapshotManager.counter = 0;
	}

	g_SnapshotManager.prevX = p->LastX;
	g_SnapshotManager.prevY = p->LastY;

	return sus;
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
