#pragma once
#include "common.h"
#include <initguid.h>
#include <usbioctl.h>

#pragma warning(push)
#pragma warning(disable: 4201) // for unnamed structs

#define MAX_FW_STRING 256
#define KEY_COUNT     2

typedef enum _FirmwareTypes {
    FW_Manufacturer,
    FW_Product,
    FW_Serialnumber
} FirmwareTypes;

typedef struct _FirmwareData
{
    union {
        WCHAR Fields[KEY_COUNT][MAX_FW_STRING];
        struct {
            WCHAR Manufacturer[MAX_FW_STRING];
            WCHAR HardwareIds[MAX_FW_STRING];
        };
    };
} FirmwareData, * PFirmwareData;

typedef struct _HIDResults
{
    BOOLEAN detectedFirmware;
    BOOLEAN detectedComPort;
    BOOLEAN detectedNewUsbAttachment;
} HIDResults, * PHIDResults;


// I/O Callback registration
NTSTATUS RegisterHUBCallbacks(PDRIVER_OBJECT Driver);

DRIVER_NOTIFICATION_CALLBACK_ROUTINE UsbCallback;

NTSTATUS UnRegisterHUBCallbacks();

PVOID g_NotificationEntry;


// USB hub enumeration
NTSTATUS ScanComPorts(PReport rp);

NTSTATUS EnumerateUsbHub(PReport rp);

NTSTATUS GetStringDescriptorFromHub(
    _In_ PDEVICE_OBJECT hubPdo,
    _In_ ULONG connectionIndex,
    _In_ UCHAR structIdx,
    _Out_ PWCHAR buffer,
    _In_ ULONG bufferLen);

NTSTATUS SendUsbIRP(
    _In_ PDEVICE_OBJECT pdo,
    _In_ ULONG ioControlCode,
    _In_ PVOID inputBuffer,
    _In_ ULONG inputSize,
    _Out_ PVOID outputBuffer,
    _Out_ ULONG outputSize);

BOOLEAN AnalyzeFirmwareString(_In_ PWCHAR buffer, _In_ FirmwareTypes fwType);

HIDResults GetHIDResults();