#pragma once
#include <ntddk.h>

NTSTATUS RegisterCallbacks();

OB_PREOP_CALLBACK_STATUS ObPreOpCallbackRoutine(
	_In_ PVOID RegistrationContext,
	_In_ POB_PRE_OPERATION_INFORMATION OperationInformation);

//void ObPostOpCallbackRoutine(
//	_In_ PVOID RegistrationContext,
//	_In_ POB_PRE_OPERATION_INFORMATION OperationInformation);