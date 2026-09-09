#include "io.h"
#include "callbacks.h"

#define IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20001, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20002, METHOD_BUFFERED, FILE_ANY_ACCESS)

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
		DbgPrint("Process Launched");
		break;
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	


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