#include "callbacks.h"
#include <ntstrsafe.h>
#include "common.h" 

PVOID g_callbackHandle = NULL;

CHAR g_Whitelist[WHITELIST_SIZE][MAX_PROCESS_NAME_LENGTH] =
{
    "explorer.exe",
    "discord.exe",
	"svchost.exe"
};


NTSTATUS RegisterCallbacks()
{
	OB_CALLBACK_REGISTRATION cbr;
	OB_OPERATION_REGISTRATION opr[2];
	UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"31001");
	PVOID rHandle = NULL;
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	opr[0].ObjectType = PsProcessType;
	opr[0].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	opr[0].PreOperation = ObPreOpCallbackRoutine;
	opr[0].PostOperation = NULL;

	opr[1].ObjectType = PsThreadType;
	opr[1].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	opr[1].PreOperation = ObPreOpCallbackRoutine;
	opr[1].PostOperation = NULL;

	cbr.Version = OB_FLT_REGISTRATION_VERSION;
	cbr.OperationRegistrationCount = 2;
	cbr.Altitude = altitude;
	cbr.RegistrationContext = NULL;
	cbr.OperationRegistration = opr;

	status = ObRegisterCallbacks(&cbr, &rHandle); 
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Failed to initialize Register Callbacks..");
		return status;
	}
	g_callbackHandle = rHandle;
	return status;
}


void UnRegisterCallbacks()
{
	if (g_callbackHandle)
	{
		ObUnRegisterCallbacks(g_callbackHandle);
		DbgPrint("Unregistered callbacks");
	}
}


static void StripHandle(_In_ POB_PRE_OPERATION_INFORMATION OperationInformation)
{
	ACCESS_MASK strippedAccess = SYNCHRONIZE | PROCESS_TERMINATE;
	
	if (OperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)
	{
		OperationInformation->Parameters->CreateHandleInformation.DesiredAccess = strippedAccess;
	}

	if (OperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE)
	{
		OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess = strippedAccess;
	}
}


OB_PREOP_CALLBACK_STATUS
ObPreOpCallbackRoutine(
	_In_ PVOID RegistrationContext,
	_In_ POB_PRE_OPERATION_INFORMATION OperationInformation)
{

	UNREFERENCED_PARAMETER(RegistrationContext);
	int flagged = 1; // Untrusted by default

	PEPROCESS targetProcess = OperationInformation->Object; // Newly created process/targeted process
	UCHAR* targetProcessName = PsGetProcessImageFileName(targetProcess);

	UCHAR* callerProcessName = PsGetProcessImageFileName(IoGetCurrentProcess()); // Process trying to get the handle

	DWORD PID = g_DriverExtention->PID;	// game's PiD
	if (!PID) goto end;  // game not launched (yet)

	PEPROCESS gameProcess;
	PsLookupProcessByProcessId((HANDLE)PID, &gameProcess);

	if (targetProcess != gameProcess) goto end; // Not our game being targeted

	ObDereferenceObject(gameProcess);


	//TODO: when matching PID's are encountered, dont block the access..

	if (OperationInformation->ObjectType == *PsThreadType)
	{

		// Dont block the game getting its own handle: 
		//if ((DWORD)(ULONG_PTR)PPsGetProcessId(IoGetCurrentProcess()) == PID) goto end;
		DbgPrint("Possibly malicous injection!");
	}


	else if (OperationInformation->ObjectType == *PsProcessType)
	{
		for (int i = 0; i < WHITELIST_SIZE; i++)
		{
			CHAR* wlIdx = g_Whitelist[i];
			ANSI_STRING str1;
			ANSI_STRING str2;

			RtlInitAnsiString(&str1, (PCSZ)callerProcessName);
			RtlInitAnsiString(&str2, (PCSZ)wlIdx);
			
			if (RtlCompareString(&str1, &str2, TRUE) == 0)
			{
				flagged = 0;
				break;
			}
		}

		if (flagged)
		{
			UCHAR buffer[256];
			RtlStringCchPrintfA((NTSTRSAFE_PSTR)buffer, sizeof(buffer), "Hit a callback on %s from %s", targetProcessName, callerProcessName);
			DbgPrint("%s", buffer);

			// Remove handle access
			StripHandle(OperationInformation);
		}
	}

	end: 
	return OB_PREOP_SUCCESS;
}