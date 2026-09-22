#include "io.h"
#include "callbacks.h"
#include "hashing.h"

#define IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20001, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20002, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SNAPSHOT_HASH_TEXT_SECTION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20003, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VERIFY_SNAPSHOT_HASH				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20004, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WALK_PROCESS_LIST 				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20005, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define PROCESS_LINK_OFFSET 0x448 // for version 22H2
#define MAX_PROCESSES		5000 // should be fine...


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

#define BLACKLIST_SIZE 4 
CHAR g_BlackList[BLACKLIST_SIZE][MAX_PROCESS_NAME_LENGTH] =
{
	"injector.exe",
	"dllinjector.exe",
	"cheat.exe",
	"cheatengine.exe"
	// probably more..
};

NTSTATUS WalkProcessList(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);
	PEPROCESS startProcess = PsGetCurrentProcess();

	//Bit risky to try this on other windows versions because Microsoft keeps changing their offsets...
	PLIST_ENTRY currentLink = (PLIST_ENTRY)((ULONG_PTR)startProcess + PROCESS_LINK_OFFSET); 
	PLIST_ENTRY nextLink = currentLink->Flink;

	BOOLEAN flagged = FALSE;
	int idx = 0;

	while (nextLink != currentLink) {
		PEPROCESS process = (PEPROCESS)((ULONG_PTR)nextLink - PROCESS_LINK_OFFSET);

		//HANDLE pid = PsGetProcessId(process);
		UCHAR* imageFileName = PsGetProcessImageFileName(process);

		for (int i = 0; i < BLACKLIST_SIZE; i++)
		{
			PCHAR blProcess = g_BlackList[i];
			ANSI_STRING str1;
			ANSI_STRING str2;

			RtlInitAnsiString(&str1, (PCSZ)blProcess);
			RtlInitAnsiString(&str2, (PCSZ)imageFileName);

			if (RtlCompareString(&str1, &str2, TRUE) == 0)
			{
				flagged = TRUE;
				break;
			}
		}

		if (flagged)
			break;

		// if not jump to the next link in the chain
		nextLink = nextLink->Flink;
		idx++;

		if (idx > MAX_PROCESSES)
			break; // something probably went wrong, dont want to spend more time in this loop
	}

	if (flagged)
		DbgPrint("Flagged a process!");

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
	case IOCTL_WALK_PROCESS_LIST:
		WalkProcessList(DeviceObject, Irp);
		//goto end; TODO: implement IRP handling for process scanning.
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