#include "driver.h"
#include "io.h"
#include "callbacks.h"
#include <stdlib.h>
#include "common.h"


UNICODE_STRING g_DeviceName = RTL_CONSTANT_STRING(L"\\Device\\KernelAC");
UNICODE_STRING g_DeviceSymbolicLink = RTL_CONSTANT_STRING(L"\\??\\KernelAC");

PDRIVER_SETTINGS g_DriverExtention = NULL;


NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject,
    IN PUNICODE_STRING RegistryPath)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    UNREFERENCED_PARAMETER(RegistryPath);


    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseHandler;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseHandler;
    pDriverObject->DriverUnload = DriverUnload;

    // TODO: Make sure some mutex guarding takes place
    DbgPrint("Entering DriverEntry");


    status = IoCreateDevice(
        pDriverObject, 
        sizeof(DRIVER_SETTINGS),
        &g_DeviceName, 
        FILE_DEVICE_UNKNOWN, 
        FILE_DEVICE_SECURE_OPEN, 
        FALSE, 
        &pDriverObject->DeviceObject);

    g_DriverExtention = pDriverObject->DeviceObject->DeviceExtension;

    if (!NT_SUCCESS(status)) {
        DbgPrint("IoCreateDevice failed, status %x", status);
        return status;
    }
    DbgPrint("IoCreateDevice succeeded");


    status = IoCreateSymbolicLink(&g_DeviceSymbolicLink, &g_DeviceName);

    if (!NT_SUCCESS(status)) {
        DbgPrint("IoCreateSymbolicLink failed, status %x", status);

        IoDeleteDevice(pDriverObject->DeviceObject);
        return status;
    }
    DbgPrint("IoCreateSymbolicLink succeeded");


    status = RegisterCallbacks();
    if (!NT_SUCCESS(status)) {
        DbgPrint("RegisterCallbacks failed, status %x", status);

        IoDeleteDevice(pDriverObject->DeviceObject);
        IoDeleteSymbolicLink(&g_DeviceSymbolicLink);
        return status;
    }
    DbgPrint("RegisterCallbacks succeeded");



    DbgPrint("-----------------------");
    DbgPrint("Driver Entry Succeeded.");
    return STATUS_SUCCESS;
}

NTSTATUS DriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    UnRegisterCallbacks();

    status = IoDeleteSymbolicLink(&g_DeviceSymbolicLink);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to delete SymbolicLink, status %x", status);
    }

    if (pDriverObject->DeviceObject)
    {
        IoDeleteDevice(pDriverObject->DeviceObject);
        DbgPrint("Deleted Device");
    }
    DbgPrint("Stopped Driver");

    return STATUS_SUCCESS;
}