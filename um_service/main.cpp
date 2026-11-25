#include <iostream>
#include <string>
#include <Windows.h>
#include "main.h"

#include <chrono>
#include <thread>

#define IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20001, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20002, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SNAPSHOT_HASH_TEXT_SECTION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20003, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_GAME_REGISTERED_CLICK				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20004, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RUNTIME_VALIDATION				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20005, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define FEATURE_MOUSE_MONITORING

int main()
{
    if (!InitializeDriver())
    {
        std::cout << "Could not start Anti Cheat";
        return 0;
    }
    if (!InitializeFilterDriver())
    {
        std::cout << "Could not register Filter";
        return 0;
    }

    // restarts the device chain so the filterdriver is enabled
    system("devcon.exe restart =Mouse");

    // TODO: INSTALL DEVCON.EXE and run it from there

    HANDLE hDriver = CreateFileW(
        L"\\\\.\\KernelAC",          
        GENERIC_READ | GENERIC_WRITE,
        0,                           
        NULL,                        
        OPEN_EXISTING,               
        FILE_ATTRIBUTE_NORMAL,       
        NULL                         
    );
    
    if (hDriver == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to get driver handle. Error: " << GetLastError() << std::endl;
    }
    else {
        DeviceIoControl(hDriver, IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE, NULL, NULL, NULL, NULL, NULL, NULL);
    }

    HANDLE pHandle;
    std::string procpath = "C:/Users/malware/Desktop/Release/game.exe";
    //std::string procpath = "C:/Windows/System32/notepad.exe";
    pHandle = CreateNewProcess(procpath);
    if (!pHandle)
    {
        std::cout << "WARNING: Could not start process! \n";
        return 0;
    }    
    DWORD PID = GetProcessId(pHandle);

    CopyOwnFileTo("dllservice.dll", "C:/Windows/System32/");
    InjectDLL(PID, "dllservice.dll");


    if (hDriver == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to get driver handle. Error: " << GetLastError() << std::endl;
    }
    else {
        DeviceIoControl(hDriver, IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH, &PID, sizeof(DWORD), NULL, NULL, NULL, NULL);
        DeviceIoControl(hDriver, IOCTL_SNAPSHOT_HASH_TEXT_SECTION, NULL, NULL, NULL, NULL, NULL, NULL);
    }


    using namespace std::chrono_literals;

    {
        RuntimeValidators mask = static_cast<RuntimeValidators>(
            VERIFY_SNAPSHOT_HASH | WALK_PROCESS_LIST | SCAN_FOR_MANUALLY_MAPPED_CODE | 
            GET_CURRENT_STATUS | CHECK_HWBP | ENUMERATE_USB_HUB | SCAN_USED_COM_PORTS);

        Report report = { 0 };
        DWORD bytesReturned = 0;

        if (hDriver)DeviceIoControl(hDriver, IOCTL_RUNTIME_VALIDATION, &mask, 
            sizeof(mask), &report, sizeof(report), &bytesReturned, NULL);

        if (bytesReturned == sizeof(report) && report.SuccessfulReport)
        {
            std::cout << "Success: " << (bool)report.SuccessfulReport << std::endl;
            std::cout << "Value 1: " << (bool)report.FlaggedNewUsbAttachment << std::endl;
            std::cout << "Value 2: " << (bool)report.FlaggedProcess << std::endl;
            std::cout << "Value 3: " << (bool)report.FlaggedUsbDevice << std::endl;
            std::cout << "Value 4: " << (bool)report.HwbpDetected << std::endl;
            std::cout << "Value 5: " << (bool)report.InvalidHash << std::endl;
            std::cout << "Value 6: " << (bool)report.ManuallyMappedCode << std::endl;
        }
    }


#ifdef FEATURE_MOUSE_MONITORING
    if (!CreateServer())
        return 0;

    while (true) 
    {
        if (IsMouseClicked())
        {
            if (hDriver)DeviceIoControl(hDriver, IOCTL_GAME_REGISTERED_CLICK, NULL, NULL, NULL, NULL, NULL, NULL);
        }
    }
#endif // FEATURE_MOUSE_MONITORING

    if(hDriver) CloseHandle(hDriver);
    UnloadDriver();

    return 1;
}