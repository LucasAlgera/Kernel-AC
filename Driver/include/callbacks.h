#pragma once
#include <ntifs.h>
#include "common.h"

NTSTATUS RegisterCallbacks();

void UnRegisterCallbacks();

OB_PREOP_CALLBACK_STATUS ObPreOpCallbackRoutine(
	_In_ PVOID RegistrationContext,
	_In_ POB_PRE_OPERATION_INFORMATION OperationInformation);
