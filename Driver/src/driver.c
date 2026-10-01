#include "driver.h"
#include "io.h"
#include "callbacks.h"
#include <stdlib.h>
#include "common.h"
#include "moufltr.h"


UNICODE_STRING g_DeviceName = RTL_CONSTANT_STRING(L"\\Device\\KernelAC");
UNICODE_STRING g_DeviceSymbolicLink = RTL_CONSTANT_STRING(L"\\??\\KernelAC");

PDRIVER_SETTINGS g_DriverExtention = NULL;
PFDEVICE_EXTENSION g_FilterDeviceExtension = NULL;

NTSTATUS DriverAddDevice(IN DRIVER_OBJECT* pDriverObject, IN DEVICE_OBJECT* pPhysicalDeviceObject)
{
    UNREFERENCED_PARAMETER(pDriverObject);

    NTSTATUS status;
    PDEVICE_OBJECT functionalDeviceObject = NULL;

    DbgPrint("MyDriver: AddDevice PDO=%p\n", pPhysicalDeviceObject);

    //
    // Create a functional device object that matches the PDO device type.
    // Use NULL for a name (filter device) and copy important characteristics
    // from the lower device so the stack behavior remains consistent.
    //
    status = IoCreateDevice(
        pDriverObject,
        sizeof(FDEVICE_EXTENSION),
        NULL,
        pPhysicalDeviceObject->DeviceType, // match the real device type
        0,
        FALSE,
        &functionalDeviceObject
    );

    if (!NT_SUCCESS(status)) {
        KdPrint("Failed to create filter device!");
        return status;
    }

    PFDEVICE_EXTENSION devExt = (PFDEVICE_EXTENSION)functionalDeviceObject->DeviceExtension;
    RtlZeroMemory(devExt, sizeof(FDEVICE_EXTENSION));

    devExt->DeviceObject = functionalDeviceObject;
    devExt->PhysicalDeviceObject = pPhysicalDeviceObject;

    //
    // Attach to the device stack and save the pointer.
    //
    devExt->NextLowerDeviceObject = IoAttachDeviceToDeviceStack(
        functionalDeviceObject,
        pPhysicalDeviceObject
    );

    if (devExt->NextLowerDeviceObject == NULL) {
        KdPrint("IoAttachDeviceToDeviceStack failed..");
        IoDeleteDevice(functionalDeviceObject);
        return STATUS_DEVICE_REMOVED;
    }

    //
    // Copy important flags, characteristics and stack size from the lower device.
    // This prevents changing behavior expected by the rest of the stack and avoids
    // stopping I/O (common cause of lost input).
    //
    functionalDeviceObject->Characteristics = pPhysicalDeviceObject->Characteristics;
    functionalDeviceObject->AlignmentRequirement = pPhysicalDeviceObject->AlignmentRequirement;
    functionalDeviceObject->Flags |= (devExt->NextLowerDeviceObject->Flags &
        (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE));
    functionalDeviceObject->StackSize = devExt->NextLowerDeviceObject->StackSize + 1;
    //
    // We're ready to receive IRPs.
    //
    functionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    //
    // Keep a module-global pointer to the last-created filter extension if you need it.
    //
    g_FilterDeviceExtension = devExt;

    DbgPrint("DriverAddDevice succeeded: FDO=%p, NextLower=%p\n",
             functionalDeviceObject, devExt->NextLowerDeviceObject);

    return STATUS_SUCCESS;
}


// Currently i am softlocked because i dont pass on IRP requests, so mouse input doesnt do anything

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject,
    IN PUNICODE_STRING RegistryPath)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    UNREFERENCED_PARAMETER(RegistryPath);


    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
        pDriverObject->MajorFunction[i] = PassIRP;
    }


    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseHandler;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseHandler;
    pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = CreateCloseHandler;
    pDriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = InternalDispatchDeviceControl;
    pDriverObject->MajorFunction[IRP_MJ_READ] = PassIRP;
    pDriverObject->MajorFunction[IRP_MJ_POWER] = PassPowerIRP;
    pDriverObject->MajorFunction[IRP_MJ_PNP] = PassIRP;
    pDriverObject->DriverUnload = DriverUnload;
    pDriverObject->DriverExtension->AddDevice = DriverAddDevice;
    // TODO: add DriverObject->DriverExtension->AddDevice and IRP_MJ_PNP!! otherwise filter wont work

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
    g_DriverExtention->DeviceObject = pDriverObject->DeviceObject;

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