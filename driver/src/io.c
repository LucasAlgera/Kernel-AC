/*
This file mostly contains I/O handling, though some smaller Anti Cheat tasks also get done here:
- walking process list 
- scanning code mapping
- handling process launching
- reporting AC status
*/

#include "io.h"
#include "callbacks.h"
#include "hashing.h"
#include "moufltr.h"
#include "proc.h"
#include <kbdmou.h> 

#define IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20001, METHOD_BUFFERED, FILE_ANY_ACCESS) // should not be any access!
#define IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20002, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SNAPSHOT_HASH_TEXT_SECTION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20003, METHOD_BUFFERED, FILE_ANY_ACCESS)

// These should be mashed in 1 IOCTL?
#define IOCTL_VERIFY_SNAPSHOT_HASH				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20004, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WALK_PROCESS_LIST 				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20005, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCAN_FOR_MANUALLY_MAPPED_CODE		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20006, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_CURRENT_STATUS				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20007, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CHECK_HWBP						CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20008, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GAME_REGISTERED_CLICK				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20009, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS DispatchDeviceControl(DEVICE_OBJECT* DeviceObject, IRP* Irp)
/*
IOCTL dispatcher for user-mode commands. 

Used by: 
	- Deviceless driver
	- Filter driver ?
*/
{
	// Is the IRP for the Anti Cheat driver?
	if (IsACDevice(DeviceObject))
	{
		PIO_STACK_LOCATION irpSp;
		ULONG IOCTL_CODE = 0;
		NTSTATUS status = STATUS_UNSUCCESSFUL;

		UNREFERENCED_PARAMETER(DeviceObject);

		irpSp = IoGetCurrentIrpStackLocation(Irp);
		IOCTL_CODE = irpSp->Parameters.DeviceIoControl.IoControlCode;

		switch (IOCTL_CODE)
		{
		case IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE:
			DbgPrint("[AC] Process Terminated");
			break;
		case IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH:
			status = HandleProcessLaunch(Irp);
			DbgPrint("[AC] Process Launched");
			break;
		case IOCTL_SNAPSHOT_HASH_TEXT_SECTION:
			status = TakeHashSnapshot(DeviceObject, Irp);
			break;
		case IOCTL_VERIFY_SNAPSHOT_HASH:
			status = VerifyHashSnapshot(DeviceObject, Irp);
			goto end;
			break;
		case IOCTL_WALK_PROCESS_LIST:
			status = WalkProcessList(DeviceObject, Irp);
			//goto end; TODO: implement IRP handling for process scanning.
			break;
		case IOCTL_SCAN_FOR_MANUALLY_MAPPED_CODE:
			status = ScanManuallyMappedCode(DeviceObject, Irp);
			break;
		case IOCTL_GET_CURRENT_STATUS: 
			status = ReportCurrentStatus(DeviceObject, Irp);
			goto end;
			break;
		case IOCTL_CHECK_HWBP:
			status = ScanThreadsForHWBP(DeviceObject, Irp);
			break;
		case IOCTL_GAME_REGISTERED_CLICK:
			HandleGameMouseClick();
			status = STATUS_SUCCESS;
			break;
		}

		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return status;
	}
	else
	{
		return PassIRP(DeviceObject, Irp);
	}

end:
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS InternalDispatchDeviceControl(DEVICE_OBJECT* DeviceObject, IRP* Irp)
/*
IOCTL dispatcher for driver-to-driver commands.
 
Used by:
	- Filter driver
*/
{
	PIO_STACK_LOCATION irpSp;
	ULONG IOCTL_CODE = 0;

	UNREFERENCED_PARAMETER(DeviceObject);

	irpSp = IoGetCurrentIrpStackLocation(Irp);
	IOCTL_CODE = irpSp->Parameters.DeviceIoControl.IoControlCode;
	switch (IOCTL_CODE)
	{
	case IOCTL_INTERNAL_MOUSE_CONNECT:
	{
		PCONNECT_DATA connectData =
			(PCONNECT_DATA)irpSp->Parameters.DeviceIoControl.Type3InputBuffer;

		if (connectData == NULL || connectData->ClassService == NULL) 
		{
			Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return STATUS_INVALID_PARAMETER;
		}

		((PFDEVICE_EXTENSION)DeviceObject->DeviceExtension)->UpperConnectData = *connectData;

		connectData->ClassService = (PVOID)MouseCallback;
		connectData->ClassDeviceObject = DeviceObject;

		break;
	}
	}
	return PassIRP(DeviceObject, Irp);
}

NTSTATUS CreateCloseHandler(DEVICE_OBJECT* DeviceObject, IRP* Irp)
/*
Generic cleanup/pass along function

Used by:
	- Deviceless driver
	- Filter driver
*/
{
	
	// Is the IRP for the Anti Cheat driver?
	if (IsACDevice(DeviceObject))
	{
		UNREFERENCED_PARAMETER(DeviceObject);

		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return STATUS_SUCCESS;
	}
	else
	{
		IoSkipCurrentIrpStackLocation(Irp);
		PFDEVICE_EXTENSION devExt = (PFDEVICE_EXTENSION)DeviceObject->DeviceExtension;
		return IoCallDriver(devExt->NextLowerDeviceObject, Irp);
	}
}

NTSTATUS ReportCurrentStatus(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	SAFETY_STATUS status = *g_DriverExtention->SafetyStatus;
	PIO_STACK_LOCATION ioStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG buffOutLength = ioStack->Parameters.DeviceIoControl.OutputBufferLength;
	PVOID buffOut = Irp->AssociatedIrp.SystemBuffer;

	if (buffOutLength < sizeof(status))
	{
		Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return STATUS_UNSUCCESSFUL;
	}

	RtlCopyMemory(buffOut, &status, sizeof(status));
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = sizeof(status);
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}