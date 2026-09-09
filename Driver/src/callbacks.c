#include "callbacks.h"

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


	return status;
}

OB_PREOP_CALLBACK_STATUS
ObPreOpCallbackRoutine(
	_In_ PVOID RegistrationContext,
	_In_ POB_PRE_OPERATION_INFORMATION OperationInformation)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	UNREFERENCED_PARAMETER(OperationInformation);
	DbgPrint("Hit a callback!");
	return OB_PREOP_SUCCESS;
}