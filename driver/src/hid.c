#include "hid.h"
#include <initguid.h>
#include <usbiodef.h>
#include <hidclass.h>
#include <devpkey.h>

#define GUID_COUNT 3
HIDResults g_HIDResults = { 0 };

VOID RegisterHUBCallbacks()
{
    // IoRegisterPlugPlayNotification?
    return;
}

DEFINE_GUID(GUID_DEVINTERFACE_MOUSE, 0x378de44c, 0x56ef, 0x11d1,
    0xbc, 0x8c, 0x00, 0xa0, 0xc9, 0x14, 0x05, 0xdd);

const GUID* g_GUIDEnumerationList[GUID_COUNT] =
{
    &GUID_DEVINTERFACE_USB_DEVICE,
    &GUID_DEVINTERFACE_HID,
    &GUID_DEVINTERFACE_MOUSE
};

static const DEVPROPKEY g_DevPropKey_Manufacturer = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 13
};
static const DEVPROPKEY g_DevPropKey_HardwareIds = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 3
};

const DEVPROPKEY* g_PropKeys[KEY_COUNT] =
{
    &g_DevPropKey_Manufacturer,
    &g_DevPropKey_HardwareIds
};

NTSTATUS EnumerateUSBHub()
{
    PZZWSTR symbolicLinkList = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    for (int i = 0; i < GUID_COUNT; i++)
    {
        const GUID* currentGUID = g_GUIDEnumerationList[i];
        
        if (currentGUID == NULL) continue;

        status = IoGetDeviceInterfaces(currentGUID, NULL, 0, &symbolicLinkList);

        if (!NT_SUCCESS(status) || symbolicLinkList == NULL) continue;
        
        for (PWSTR currentLink = symbolicLinkList; *currentLink != L'\0'; currentLink += wcslen(currentLink) + 1)
        {
            PFILE_OBJECT pfo = NULL;
            PDEVICE_OBJECT pdo = NULL;
            UNICODE_STRING ObjectName;

            RtlInitUnicodeString(&ObjectName, currentLink);

            status = IoGetDeviceObjectPointer(&ObjectName, FILE_READ_ATTRIBUTES, &pfo, &pdo);
            PDEVICE_OBJECT actualPdo = IoGetDeviceAttachmentBaseRef(pdo); //??

            if (NT_SUCCESS(status) && pdo != NULL)
            {
                PFirmwareData fwdata;

                fwdata = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(FirmwareData), 'tdwF');
                if (fwdata == NULL) continue;

                status = RetrieveFirmware(actualPdo, fwdata);
                ObDereferenceObject(pfo);
                ObDereferenceObject(actualPdo);

                if (!NT_SUCCESS(status))
                {
                    ExFreePoolWithTag(fwdata, 'tdwF');
                    continue;
                }

                AnalyzeFirmware(fwdata);
                ExFreePoolWithTag(fwdata, 'tdwF');
            }
        }

        ExFreePool(symbolicLinkList);
        symbolicLinkList = NULL;
    }

    DbgPrint("[AC] Exiting\n");

    return status;
}

NTSTATUS RetrieveFirmware(_In_ PDEVICE_OBJECT pdo, _Out_ PFirmwareData fwdata)
{
    NTSTATUS status;
    ULONG requiredSize = 0;
    DEVPROPTYPE propType = 0;
    PVOID data = NULL;

    if (fwdata)
        RtlZeroMemory(fwdata, sizeof(FirmwareData));
    else
        return STATUS_UNSUCCESSFUL;

    for (int i = 0; i < KEY_COUNT; i++)
    {
        status = IoGetDevicePropertyData(
            pdo,
            g_PropKeys[i],
            LOCALE_NEUTRAL,
            0,
            0,
            NULL,
            &requiredSize,
            &propType
        );

        if (status == STATUS_BUFFER_TOO_SMALL && requiredSize > 0 && requiredSize <= MAX_FW_STRING * sizeof(WCHAR))
        {
            data = ExAllocatePool2(POOL_FLAG_NON_PAGED, requiredSize, 'prpD');
            if (data != NULL)
            {
                status = IoGetDevicePropertyData(
                    pdo,
                    g_PropKeys[i],
                    LOCALE_NEUTRAL,
                    0,
                    requiredSize,
                    data,
                    &requiredSize,
                    &propType
                );

                if (NT_SUCCESS(status) && (propType == DEVPROP_TYPE_STRING || propType == DEVPROP_TYPE_STRING_LIST))
                {
                    RtlCopyMemory(fwdata->Fields[i], data, min(requiredSize, MAX_FW_STRING * sizeof(WCHAR)));
                }

                ExFreePoolWithTag(data, 'prpD');
            }
        }
    }

    DbgPrint("[AC] scanned a device!\n");
    return status;
}

VOID AnalyzeFirmware(_In_ PFirmwareData fwdata)
{
    UNREFERENCED_PARAMETER(fwdata);
    //DbgPrint("[AC] Manufacturer:  %ws\n", fwdata->Manufacturer);
    //DbgPrint("[AC] HardwareIds:  %ws\n", fwdata->HardwareIds);
    return;
}

HIDResults GetHIDResults()
{
    return g_HIDResults;
}