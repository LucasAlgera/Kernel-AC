#include "hid.h"
#include <initguid.h>
#include <usbiodef.h>
#include <devpkey.h>

HIDResults g_HIDResults = { 0 };

VOID RegisterHUBCallbacks()
{
    // IoRegisterPlugPlayNotification?
    return;
}

NTSTATUS EnumerateUSBHub()
{
    PZZWSTR symbolicLinkList = NULL;
    NTSTATUS status;

    status = IoGetDeviceInterfaces(&GUID_DEVINTERFACE_USB_DEVICE, NULL, 0, &symbolicLinkList);

    DbgPrint("[AC] Check 1");

    if (NT_SUCCESS(status) && symbolicLinkList != NULL) 
    {
        for (PWSTR currentLink = symbolicLinkList; *currentLink != L'\0'; currentLink += wcslen(currentLink) + 1) 
        {
            DbgPrint("[AC] Check 2");

            PFILE_OBJECT pfo = NULL;
            PDEVICE_OBJECT pdo = NULL;
            UNICODE_STRING ObjectName;

            RtlInitUnicodeString(&ObjectName, currentLink);

            status = IoGetDeviceObjectPointer(&ObjectName, FILE_READ_ATTRIBUTES, &pfo, &pdo);
            DbgPrint("Operation failed with status: 0x%08X\n", status);
            if (NT_SUCCESS(status) && pdo != NULL)
            {
                DbgPrint("[AC] Check 3");

                FirmwareData fwdata = { 0 };

                status = RetrieveFirmware(pdo, &fwdata);
                if (!NT_SUCCESS(status)) continue;

                AnalyzeFirmware(&fwdata);
            }
        }

        ExFreePool(symbolicLinkList);
    }

    DbgPrint("[AC] Exiting");

    return status;
}

NTSTATUS RetrieveFirmware(_In_ PDEVICE_OBJECT pdo, _Out_ PFirmwareData fwdata)
{
    NTSTATUS status = STATUS_SUCCESS;
    //DEVPROPKEY key = DEVPKEY_Device_Manufacturer;
    UNREFERENCED_PARAMETER(pdo);
    UNREFERENCED_PARAMETER(fwdata);
    //status = IoGetDevicePropertyData(pdo, &key, );
    DbgPrint("[AC] scanned a device!");
    return status;
}

VOID AnalyzeFirmware(_In_ PFirmwareData fwdata)
{
    UNREFERENCED_PARAMETER(fwdata);
    return;
}

HIDResults GetHIDResults()
{
    return g_HIDResults;
}