#include "driver.h"
#include "io.h"
#include "callbacks.h"
#include <stdlib.h>
#include "common.h"
#include "moufltr.h"


UNICODE_STRING g_DeviceName = RTL_CONSTANT_STRING(L"\\Device\\KernelAC");
UNICODE_STRING g_DeviceSymbolicLink = RTL_CONSTANT_STRING(L"\\??\\KernelAC");
PDRIVER_SETTINGS g_DriverExtention = NULL;

NTSTATUS DriverAddDevice(IN DRIVER_OBJECT* pDriverObject, IN DEVICE_OBJECT* pPhysicalDeviceObject)
/*
This function routine gets called as a response to the AddDevice routine from the PnP manager. 
Our new new device should match the PDO.

NOTE: After this is ran, either a full reboot is needed or I find a way to restart specific PnP devices..
*/
{
    UNREFERENCED_PARAMETER(pDriverObject);

    NTSTATUS status;
    PDEVICE_OBJECT functionalDeviceObject = NULL;

    DbgPrint("[AC] MyDriver: AddDevice PDO=%p\n", pPhysicalDeviceObject);

    status = IoCreateDevice(
        pDriverObject,
        sizeof(FDEVICE_EXTENSION),
        NULL,
        pPhysicalDeviceObject->DeviceType, // match the real device type
        0,
        FALSE,
        &functionalDeviceObject
    );

    if (!NT_SUCCESS(status)) 
    {
        DbgPrint("[AC] Failed to create filter device!");
        return status;
    }

    // device extention mostly used for forwarding IRP's to the next device in the stack
    PFDEVICE_EXTENSION devExt = (PFDEVICE_EXTENSION)functionalDeviceObject->DeviceExtension;
    RtlZeroMemory(devExt, sizeof(FDEVICE_EXTENSION));

    devExt->DeviceObject = functionalDeviceObject;
    devExt->PhysicalDeviceObject = pPhysicalDeviceObject;

    devExt->NextLowerDeviceObject = IoAttachDeviceToDeviceStack(
        functionalDeviceObject,
        pPhysicalDeviceObject
    );

    if (devExt->NextLowerDeviceObject == NULL) 
    {
        DbgPrint("[AC] IoAttachDeviceToDeviceStack failed..");
        IoDeleteDevice(functionalDeviceObject);
        return STATUS_DEVICE_REMOVED;
    }

    // Copy flags from PDO 
    functionalDeviceObject->Characteristics = pPhysicalDeviceObject->Characteristics;
    functionalDeviceObject->AlignmentRequirement = pPhysicalDeviceObject->AlignmentRequirement;
    functionalDeviceObject->Flags |= (devExt->NextLowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE));
    functionalDeviceObject->StackSize = devExt->NextLowerDeviceObject->StackSize; // (+1)?
    
    functionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING; // Ready to recieve IRP's

    DbgPrint("[AC] DriverAddDevice succeeded: FDO=%p, NextLower=%p\n", functionalDeviceObject, devExt->NextLowerDeviceObject);

    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING RegistryPath)
/*
This function is called as the entry point of the driver, important things happening: 
- Creation of the deviceless driver (software driver) which accepts user-mode IRP's from the AC service. 
- Setup of MajorFunctions which support dual-mode drivers (filter driver + deviceless driver).
- Creates symbolic links for the AC service and registers basic callbacks. 
*/
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    UNREFERENCED_PARAMETER(RegistryPath);

    // Make sure the filter driver passes along most incoming IRP's along 
    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
        pDriverObject->MajorFunction[i] = PassIRP;
    }

    // Set all the specific functionalities
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    pDriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = InternalDispatchDeviceControl;
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseHandler;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseHandler;
    pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = CreateCloseHandler;
    pDriverObject->DriverExtension->AddDevice = DriverAddDevice;
    pDriverObject->DriverUnload = DriverUnload;

    // TODO: Make sure some mutex guarding takes place
    DbgPrint("[AC] Entering DriverEntry");


    status = IoCreateDevice(
        pDriverObject, 
        sizeof(DRIVER_SETTINGS),
        &g_DeviceName, 
        FILE_DEVICE_UNKNOWN, 
        FILE_DEVICE_SECURE_OPEN, 
        FALSE, 
        &pDriverObject->DeviceObject);


    if (pDriverObject->DeviceObject != NULL)
    {
        g_DriverExtention = pDriverObject->DeviceObject->DeviceExtension;
        g_DriverExtention->DeviceObject = pDriverObject->DeviceObject;
    }


    if (!NT_SUCCESS(status)) {
        DbgPrint("[AC] IoCreateDevice failed, status %x", status);
        return status;
    }
    DbgPrint("[AC] IoCreateDevice succeeded");

    status = IoCreateSymbolicLink(&g_DeviceSymbolicLink, &g_DeviceName);

    if (!NT_SUCCESS(status)) {
        DbgPrint("[AC] IoCreateSymbolicLink failed, status %x", status);

        IoDeleteDevice(pDriverObject->DeviceObject);
        return status;
    }
    DbgPrint("[AC] IoCreateSymbolicLink succeeded");


    status = RegisterCallbacks();
    if (!NT_SUCCESS(status)) {
        DbgPrint("[AC] RegisterCallbacks failed, status %x", status);

        IoDeleteDevice(pDriverObject->DeviceObject);
        IoDeleteSymbolicLink(&g_DeviceSymbolicLink);
        return status;
    }
    DbgPrint("[AC] RegisterCallbacks succeeded");

    DbgPrint("[AC] Driver Entry Succeeded.");
    return STATUS_SUCCESS;
}

NTSTATUS DriverUnload(IN PDRIVER_OBJECT pDriverObject)
/*
This function is kind of obsolete since the driver wont unload when it's used in PnP...
*/
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    UnRegisterCallbacks();

    status = IoDeleteSymbolicLink(&g_DeviceSymbolicLink);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[AC] Failed to delete SymbolicLink, status %x", status);
    }

    if (pDriverObject->DeviceObject)
    {
        IoDeleteDevice(pDriverObject->DeviceObject);
        DbgPrint("[AC] Deleted Device");
    }
    DbgPrint("Stopped Driver");

    return STATUS_SUCCESS;
}