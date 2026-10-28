#include "proc.h"

#define PROCESS_LINK_OFFSET 0x448 // for version 22H2
#define MAX_PROCESSES		5000  // should be fine...

#define BLACKLIST_SIZE 4 
CHAR g_BlackList[BLACKLIST_SIZE][MAX_PROCESS_NAME_LENGTH] =
{
	"injector.exe",
	"dllinjector.exe",
	"cheat.exe",
	"cheatengine.exe"
	// probably more..
};

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

NTSTATUS WalkProcessList(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);
	PEPROCESS startProcess = PsGetCurrentProcess();

	// Bit risky to try this on other windows versions because Microsoft keeps changing their offsets...
	PLIST_ENTRY currentLink = (PLIST_ENTRY)((ULONG_PTR)startProcess + PROCESS_LINK_OFFSET);
	PLIST_ENTRY nextLink = currentLink->Flink;

	BOOLEAN flagged = FALSE;
	int idx = 0;

	while (nextLink != currentLink) {
		PEPROCESS process = (PEPROCESS)((ULONG_PTR)nextLink - PROCESS_LINK_OFFSET);

		// HANDLE pid = PsGetProcessId(process);
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
		DbgPrint("[AC] Flagged a process!");

	return STATUS_SUCCESS;
}

NTSTATUS ScanManuallyMappedCode(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	PIO_STACK_LOCATION irpSp;
	ULONG IOCTL_CODE = 0;
	PEPROCESS process;

	UNREFERENCED_PARAMETER(DeviceObject);

	irpSp = IoGetCurrentIrpStackLocation(Irp);
	IOCTL_CODE = irpSp->Parameters.DeviceIoControl.IoControlCode;

	if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)g_DriverExtention->PID, &process)))
		return STATUS_UNSUCCESSFUL;

	PVOID baseAddress = NULL;
	MEMORY_BASIC_INFORMATION mbi;

	KAPC_STATE apcState;
	KeStackAttachProcess(process, &apcState);

	while (NT_SUCCESS(ZwQueryVirtualMemory(
		process,
		baseAddress,
		MemoryBasicInformation,
		&mbi,
		sizeof(mbi),
		NULL)))
	{
		if (mbi.State == MEM_COMMIT &&
			(mbi.Protect & PAGE_EXECUTE_READ ||
				mbi.Protect & PAGE_EXECUTE_READWRITE ||
				mbi.Protect & PAGE_EXECUTE_WRITECOPY) &&
			mbi.Type == MEM_PRIVATE)
		{
			DbgPrint("[AC] Found manually mapped code!");
			return STATUS_UNSUCCESSFUL;
		}
		baseAddress = (PVOID)((ULONG_PTR)mbi.BaseAddress + mbi.RegionSize);
		if ((ULONG_PTR)baseAddress >= 0x7FFFFFFFFFFF) break; // user-space limit
	}
	KeUnstackDetachProcess(&apcState);

	return STATUS_SUCCESS;
}

PFN_PsGetContextThread GetPsGetThreadContext()
/*
PsGetThreadContext is not always available in Windows since it's undocumented/unsupported. 
Try to get the address of the function by dynamically resolving it. If it's not available.. too bad.
*/
{
	// Make sure we can use PsGetThreadContext
	UNICODE_STRING routineName;
	RtlInitUnicodeString(&routineName, L"PsGetContextThread");

	PFN_PsGetContextThread addr =
		(PFN_PsGetContextThread)MmGetSystemRoutineAddress(&routineName);

	if (addr == NULL) {
		RtlInitUnicodeString(&routineName, L"PsGetThreadContext");
		addr = (PFN_PsGetContextThread)MmGetSystemRoutineAddress(&routineName);
	}

	return addr;
}

VOID ParseDebugRegisters(PCONTEXT Context, HANDLE ThreadId) {

	UNREFERENCED_PARAMETER(ThreadId);

	// Dr7 bits 0, 2, 4, 6 represent Local Enable for DR0, DR1, DR2, DR3
	// Dr7 bits 1, 3, 5, 7 represent Global Enable for DR0, DR1, DR2, DR3
	ULONG_PTR dr7 = Context->Dr7;

	BOOLEAN bp0 = (dr7 & (1 << 0)) || (dr7 & (1 << 1));
	BOOLEAN bp1 = (dr7 & (1 << 2)) || (dr7 & (1 << 3));
	BOOLEAN bp2 = (dr7 & (1 << 4)) || (dr7 & (1 << 5));
	BOOLEAN bp3 = (dr7 & (1 << 6)) || (dr7 & (1 << 7));

	if (bp0 || bp1 || bp2 || bp3) 
	{
		KdPrint(("[AC] HWBP detected on Thread %p!\n", ThreadId));
		// (PVOID)Context->Dr0-7 to view the address.

		// TODO: Update status/reporting struct here
		// TODO: Maybe disable the HWBPs?
	}
}

VOID InspectThreadContext(HANDLE ThreadId) 
{
	PETHREAD pEthread = NULL;
	NTSTATUS status;

	PFN_PsGetContextThread PsGetThreadContext = GetPsGetThreadContext();
	if (PsGetThreadContext == NULL) return; // PsGetThreadContext is probably not available for use

	status = PsLookupThreadByThreadId(ThreadId, &pEthread);
	if (!NT_SUCCESS(status)) return;

	PCONTEXT pContext = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(CONTEXT), 'Txtc');
	if (!pContext) 
	{
		ObDereferenceObject(pEthread);
		return;
	}

	pContext->ContextFlags = CONTEXT_DEBUG_REGISTERS; // saves some computing

	status = PsGetThreadContext(pEthread, pContext, KernelMode);
	if (NT_SUCCESS(status)) 
	{
		ParseDebugRegisters(pContext, ThreadId);
	}

	ExFreePoolWithTag(pContext, 'Txtc');
	ObDereferenceObject(pEthread); 
}

NTSTATUS ScanThreadsForHWBP(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	PVOID buff = NULL;
	ULONG size;
	NTSTATUS status;
	HANDLE PID = (HANDLE)(ULONG_PTR)g_DriverExtention->PID;

	status = ZwQuerySystemInformation(SystemProcessInformation, NULL, 0, &size);
	if (!NT_SUCCESS(status)) return status;

	size += 0x1000; // some padding
	if (size == 0) return STATUS_UNSUCCESSFUL;

	buff = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'drht');
	if (!buff) return STATUS_UNSUCCESSFUL;

	status = ZwQuerySystemInformation(SystemProcessInformation, buff, size, &size);
	if (!NT_SUCCESS(status)) {
		ExFreePoolWithTag(buff, 'drht');
		return status;
	}

	PSYSTEM_PROCESS_INFORMATION pCurrent = (PSYSTEM_PROCESS_INFORMATION)buff;

	while (TRUE) 
	{
		// Only inspect HWBP's for our game
		if (pCurrent->UniqueProcessId == PID) 
		{
			for (ULONG i = 0; i < pCurrent->NumberOfThreads; i++) 
			{
				SYSTEM_THREAD_INFORMATION thread = pCurrent->Threads[i];
				HANDLE tid = thread.ClientId.UniqueThread;

				InspectThreadContext(tid);
			}
			break;
		}

		if (pCurrent->NextEntryOffset == 0) break;
		pCurrent = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)pCurrent + pCurrent->NextEntryOffset);
	}

	ExFreePoolWithTag(buff, 'drht');
	return status;
}