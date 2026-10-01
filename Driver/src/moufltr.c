#include "moufltr.h"

#pragma alloc_text(NONPAGE, MouseCallback)


VOID MouseCallback(PDEVICE_OBJECT DeviceObject, PMOUSE_INPUT_DATA InputDataStart, PMOUSE_INPUT_DATA InputDataEnd, PULONG InputDataConsumed)
{
    PFDEVICE_EXTENSION ext = (PFDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    DbgPrint("Hit a mouse callback!");

    // Inspect/transform the packets here.
    for (PMOUSE_INPUT_DATA p = InputDataStart; p < InputDataEnd; ++p)
    {
        // p->Flags
        // p->ButtonFlags
        // p->ButtonData
        // p->LastX
        // p->LastY
        // etc.
    }

    (*(PSERVICE_CALLBACK_ROUTINE)ext->UpperConnectData.ClassService)(
        ext->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
    );
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
			"Device extention not initialized!\n");
		return STATUS_INVALID_DEVICE_REQUEST;
	}



	IoSkipCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	status = IoCallDriver(devExt->NextLowerDeviceObject, Irp);

	if (NT_SUCCESS(status))
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Passing IRP SUCCESS!\n");
	else
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Passing IRP FAILED!\n");

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
			"Device extention not initialized!\n");
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);

	return PoCallDriver(devExt->NextLowerDeviceObject, Irp);
}

BOOLEAN IsACDevice(DEVICE_OBJECT* DeviceObject)
{
	if (DeviceObject == g_DriverExtention->DeviceObject)
		return TRUE;
	else
		return FALSE;
}
