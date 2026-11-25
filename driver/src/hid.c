/*
Functions in this file handle everything related to USB/HID detection. 
Normally the now hardcoded blocklists would either be encrypted in memory or handled on the server.
(For now) I just store them here..
*/

#include "hid.h"
#include <wdmguid.h>

#define MANUFACTURER_SIZE 6
#define PRODUCT_SIZE 5
#define SERIALNUMBER_SIZE 8

// can't include both winioctl.h and usbioctl.h so had to manually include this GUID..
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT, 0X86E0D1E0L, 0X8089, 0X11D0, 0X9C, 0XE4, 0X08, 0X00, 0X3E, 0X30, 0X1F, 0X73);

HIDResults g_HIDResults = { 0 };

PVOID g_NotificationEntry = NULL;

// These blocklists are known to be used in cheats, these results should not be weighed extremely harsly. 
// But they could indicate something..
const PCWSTR g_BlockedManufacturer[MANUFACTURER_SIZE] =
{
    L"Cheat",
    L"SparkFun", 
    L"Raspberry Pi",
    L"Espressif",
    L"TinyUSB",
    L"Teensyduino"
};

const PCWSTR g_BlockedProduct[PRODUCT_SIZE] =
{
    L"Arduino Leonardo",
    L"SparkFun Pro Micro",
    L"Pico",
    L"TinyUSB Device",
    L"ESP32-S3"
};

const PCWSTR g_BlockedSerialnumber[SERIALNUMBER_SIZE] =
{
    L"",
    L"123456",
    L"1234567",
    L"12345678",
    L"123456789",
    L"1234567890",
    L"fedcba9876543210",
    L"0123456789abcdef",
};


NTSTATUS RegisterHUBCallbacks(PDRIVER_OBJECT Driver)
{
    NTSTATUS status = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange, 
        PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        (PVOID)&GUID_DEVINTERFACE_USB_DEVICE, Driver,
        &UsbCallback, NULL, &g_NotificationEntry);

    return status;
}

NTSTATUS UsbCallback(
    _In_ PVOID NotificationStructure,
    _Inout_opt_ PVOID Context
) 
{
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION Notification = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;
    NTSTATUS status = STATUS_NOT_SUPPORTED;

    UNREFERENCED_PARAMETER(Context);

    if (IsEqualGUID(&Notification->Event, &GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        Report rp;
        status = EnumerateUsbHub(&rp);
        if (rp.FlaggedUsbDevice)
            g_HIDResults.detectedNewUsbAttachment = TRUE;

        DbgPrint("[AC] New Device attached!\n");
    }

    return status;
}

NTSTATUS UnRegisterHUBCallbacks()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    if (g_NotificationEntry != NULL)
    {
        status = IoUnregisterPlugPlayNotificationEx(g_NotificationEntry);
        g_NotificationEntry = NULL;
    }
    return status;
}

NTSTATUS ScanComPorts()
{
    PAGED_CODE();

    PZZWSTR symbolicLinkList = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    status = IoGetDeviceInterfaces(&GUID_DEVINTERFACE_COMPORT, NULL, 0, &symbolicLinkList);

    if (!NT_SUCCESS(status) || symbolicLinkList == NULL) return status;

    for (PWSTR currentLink = symbolicLinkList; *currentLink != L'\0'; currentLink += wcslen(currentLink) + 1)
    {
        UNICODE_STRING portName;
        OBJECT_ATTRIBUTES objAttr;
        IO_STATUS_BLOCK ioStatus;
        HANDLE hPort = NULL;

        RtlInitUnicodeString(&portName, currentLink);

        InitializeObjectAttributes(&objAttr, &portName, 
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwCreateFile(&hPort, GENERIC_READ | GENERIC_WRITE, &objAttr, &ioStatus, 
            NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, 0);

        if (status == STATUS_ACCESS_DENIED || status == STATUS_SHARING_VIOLATION)
        {
            g_HIDResults.detectedComPort = TRUE;
        }
        if (hPort)
        {
            ZwClose(hPort);
        }
    }

    ExFreePool(symbolicLinkList);

    return STATUS_SUCCESS;
}

NTSTATUS EnumerateUsbHub(PReport rp)
/*
- Goes through all USB hubs and enumerates through every open usb connection. 
- Retrieves some firmware values and compares them against a blacklist.
*/
{
    PAGED_CODE();

    PZZWSTR symbolicLinkList = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
 
    if (rp->FlaggedNewUsbAttachment == FALSE && g_HIDResults.detectedNewUsbAttachment == TRUE)
    {
        rp->FlaggedNewUsbAttachment = g_HIDResults.detectedNewUsbAttachment;
    }

    status = IoGetDeviceInterfaces(&GUID_DEVINTERFACE_USB_HUB, NULL, 0, &symbolicLinkList);
 
    if (!NT_SUCCESS(status) || symbolicLinkList == NULL) return status;
    
    for (PWSTR currentLink = symbolicLinkList; *currentLink != L'\0'; currentLink += wcslen(currentLink) + 1)
    {
        PFILE_OBJECT pfo = NULL;
        PDEVICE_OBJECT pdo = NULL;
        UNICODE_STRING ObjectName;
 
        RtlInitUnicodeString(&ObjectName, currentLink);
 
        status = IoGetDeviceObjectPointer(&ObjectName, FILE_READ_ATTRIBUTES, &pfo, &pdo);
        if (!NT_SUCCESS(status) || pdo == NULL)
        {
            continue;
        }

        USB_NODE_INFORMATION nodeInfo = { 0 };
        nodeInfo.NodeType = UsbHub;

        status = SendUsbIRP(pdo, IOCTL_USB_GET_NODE_INFORMATION, &nodeInfo, 
            sizeof(nodeInfo), &nodeInfo, sizeof(nodeInfo));
        if(!NT_SUCCESS(status)) continue;

        UCHAR numberOfPorts = nodeInfo.u.HubInformation.HubDescriptor.bNumberOfPorts;

        for (ULONG i = 1; i <= numberOfPorts; i++)
        {
            USB_NODE_CONNECTION_INFORMATION_EX connInfo = { 0 };
            connInfo.ConnectionIndex = i;

            status = SendUsbIRP(pdo, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                &connInfo, sizeof(connInfo), &connInfo, sizeof(connInfo));
            if (!NT_SUCCESS(status) || connInfo.ConnectionStatus != DeviceConnected) continue;

            WCHAR manufacturer[MAX_FW_STRING] = { 0 };
            WCHAR product[MAX_FW_STRING] = { 0 };
            WCHAR serialnumber[MAX_FW_STRING] = { 0 };

            if (connInfo.DeviceDescriptor.iManufacturer)
            {
                GetStringDescriptorFromHub(pdo, i, connInfo.DeviceDescriptor.iManufacturer,
                    manufacturer, sizeof(manufacturer));
                AnalyzeFirmwareString(manufacturer, FW_Manufacturer);
            }

            if (connInfo.DeviceDescriptor.iProduct)
            {
                GetStringDescriptorFromHub(pdo, i, connInfo.DeviceDescriptor.iProduct,
                    product, sizeof(product));
                AnalyzeFirmwareString(product, FW_Product);
            }
            
            if (connInfo.DeviceDescriptor.iSerialNumber)
            {
                GetStringDescriptorFromHub(pdo, i, connInfo.DeviceDescriptor.iSerialNumber,
                    serialnumber, sizeof(serialnumber));
                AnalyzeFirmwareString(serialnumber, FW_Serialnumber);
            }
            
            if (g_HIDResults.detectedFirmware && rp != NULL)
            {
                rp->FlaggedUsbDevice = TRUE;
                DbgPrint("[AC] Suspicous firmware entry \n");
            }
        }

        ObDereferenceObject(pfo);
    }
 
    ExFreePool(symbolicLinkList);
    symbolicLinkList = NULL;
 
    return status;
}

NTSTATUS GetStringDescriptorFromHub(
    _In_ PDEVICE_OBJECT pdo,
    _In_ ULONG connectionIndex,
    _In_ UCHAR structIdx,
    _Out_ PWCHAR buffer,
    _In_ ULONG bufferLen)
/*
- Requests a string descriptor from the specified connection index inside of the USB hub. 
*/
{
    ULONG requestSize = sizeof(USB_DESCRIPTOR_REQUEST) + bufferLen;
    PUSB_DESCRIPTOR_REQUEST request = ExAllocatePool2(POOL_FLAG_NON_PAGED, requestSize, 'qrsd');
    if (!request) return STATUS_INSUFFICIENT_RESOURCES;

    // USB desc request settings
    request->ConnectionIndex = connectionIndex;
    request->SetupPacket.bmRequest = 0x80;
    request->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    request->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8) | structIdx;
    request->SetupPacket.wIndex = 0x0409; // English request
    request->SetupPacket.wLength = (USHORT)bufferLen;

    NTSTATUS status = SendUsbIRP(pdo, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
        request, requestSize, request, requestSize);

    if (NT_SUCCESS(status))
    {
        PUSB_STRING_DESCRIPTOR desc = (PUSB_STRING_DESCRIPTOR)request->Data;
        
        if (desc->bLength > sizeof(USB_COMMON_DESCRIPTOR))
        {
            ULONG len = min(desc->bLength - sizeof(USB_COMMON_DESCRIPTOR), bufferLen);
            RtlCopyMemory(buffer, desc->bString, len);
        }
    }

    ExFreePoolWithTag(request, 'qrsd');
    return status;
}

NTSTATUS SendUsbIRP(
    _In_ PDEVICE_OBJECT pdo,
    _In_ ULONG ioControlCode,
    _In_ PVOID inputBuffer,
    _In_ ULONG inputSize,
    _Out_ PVOID outputBuffer,
    _Out_ ULONG outputSize)
{
    KEVENT event;
    IO_STATUS_BLOCK ioStatus;
    PIRP irp;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(
        ioControlCode,
        pdo,
        inputBuffer,
        inputSize,
        outputBuffer,
        outputSize,
        FALSE,
        &event,
        &ioStatus
    );

    if (!irp)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS status = IoCallDriver(pdo, irp);

    if (status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    return status;
}

BOOLEAN AnalyzeFirmwareString(_In_ PWCHAR buffer, _In_ FirmwareTypes fwType)
{
    switch (fwType)
    {
    case FW_Manufacturer:
        for (int i = 0; i < MANUFACTURER_SIZE; i++)
        {
            if (_wcsicmp(buffer, g_BlockedManufacturer[i]) == 0)
            {
                g_HIDResults.detectedFirmware = TRUE;
                return TRUE;
            }
        }
        break;
    case FW_Product:
        for (int i = 0; i < PRODUCT_SIZE; i++)
        {
            if (_wcsicmp(buffer, g_BlockedProduct[i]) == 0)
            {
                g_HIDResults.detectedFirmware = TRUE;
                return TRUE;
            }
        }
        break;
    case FW_Serialnumber:
        for (int i = 0; i < SERIALNUMBER_SIZE; i++)
        {
            if (_wcsicmp(buffer, g_BlockedSerialnumber[i]) == 0)
            {
                g_HIDResults.detectedFirmware = TRUE;
                return TRUE;
            }
        }
        break;
    }

    return FALSE;
}

HIDResults GetHIDResults()
{
    return g_HIDResults;
}