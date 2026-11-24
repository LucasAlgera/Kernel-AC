#include "hid.h"

#define GUID_COUNT 4
#define BLOCKLIST_SIZE 1

// can't include both winioctl.h and usbioctl.h so had to manually include this GUID..
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT, 0X86E0D1E0L, 0X8089, 0X11D0, 0X9C, 0XE4, 0X08, 0X00, 0X3E, 0X30, 0X1F, 0X73);

HIDResults g_HIDResults = { 0 };

const UNICODE_STRING g_Blacklist[BLOCKLIST_SIZE] =
{
    RTL_CONSTANT_STRING(L"Cheat"),
};

VOID RegisterHUBCallbacks()
{
    // IoRegisterPlugPlayNotification?
    return;
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

NTSTATUS EnumerateUsbHub()
/*
- Goes through all USB hubs and enumerates through every open usb connection. 
- Retrieves some firmware values and compares them against a blacklist.
*/
{
    PAGED_CODE();

    PZZWSTR symbolicLinkList = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
 
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
                GetStringDescriptorFromHub(pdo, i, connInfo.DeviceDescriptor.iManufacturer,
                    manufacturer, sizeof(manufacturer));

            if (connInfo.DeviceDescriptor.iProduct)
                GetStringDescriptorFromHub(pdo, i, connInfo.DeviceDescriptor.iProduct,
                    product, sizeof(product));

            if (connInfo.DeviceDescriptor.iSerialNumber)
                GetStringDescriptorFromHub(pdo, i, connInfo.DeviceDescriptor.iSerialNumber,
                    serialnumber, sizeof(serialnumber));

            if (AnalyzeFirmwareString(manufacturer))
            {
                g_HIDResults.detectedFirmware = TRUE;
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

BOOLEAN AnalyzeFirmwareString(_In_ PWCHAR buffer)
{
    UNICODE_STRING strBuffer;
    RtlInitUnicodeString(&strBuffer, buffer);

    for (int i = 0; i < BLOCKLIST_SIZE; i++)
    {
        if (RtlEqualUnicodeString(&g_Blacklist[i], &strBuffer, FALSE))
        {
            return TRUE;
        }
    }
    return FALSE;
}

HIDResults GetHIDResults()
{
    return g_HIDResults;
}