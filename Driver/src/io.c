#include "io.h"
#include "callbacks.h"
#include "hashing.h"

#define IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20001, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20002, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SNAPSHOT_HASH_TEXT_SECTION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20003, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VERIFY_SNAPSHOT_HASH				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20004, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS HandleProcessLaunch(IRP* Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	ULONG inBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
	PVOID lpBufferIn = Irp->AssociatedIrp.SystemBuffer;

	if (inBufferLength != sizeof(4)) // <- DWORD Length
	{
		return STATUS_UNSUCCESSFUL;
	}

	unsigned long PID = *(unsigned long*)lpBufferIn;
	g_DriverExtention->PID = PID;
	return STATUS_SUCCESS;
}


NTSTATUS DispatchDeviceControl(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	PIO_STACK_LOCATION stackSpace;
	ULONG IOCTL_CODE = 0;

	UNREFERENCED_PARAMETER(DeviceObject);

	stackSpace = IoGetCurrentIrpStackLocation(Irp);
	IOCTL_CODE = stackSpace->Parameters.DeviceIoControl.IoControlCode;
	switch (IOCTL_CODE)
	{
	case IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE:
		DbgPrint("Process Terminated");
		break;
	case IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH:
		HandleProcessLaunch(Irp);
		DbgPrint("Process Launched");
		break;
	case IOCTL_SNAPSHOT_HASH_TEXT_SECTION:
		TakeHashSnapshot(DeviceObject, Irp);
		break;
	case IOCTL_VERIFY_SNAPSHOT_HASH:
		VerifyHashSnapshot(DeviceObject, Irp);
		goto end;
		break;
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

end:
	return STATUS_SUCCESS;
}

NTSTATUS CreateCloseHandler(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	

	return STATUS_SUCCESS;
}