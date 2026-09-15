#include <iostream>
#include <string>
#include <Windows.h>
#include "main.h"

#define IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20001, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20002, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SNAPSHOT_HASH_TEXT_SECTION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20003, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VERIFY_SNAPSHOT_HASH				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20004, METHOD_BUFFERED, FILE_ANY_ACCESS)


int main()
{
    if (!InitializeDriver())
    {
        //UnloadDriver();
        std::cout << "Could not start Anti Cheat";
        return 0;
    }


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
    pHandle = CreateNewProcess("C:/Windows/system32/notepad.exe");
    if (!pHandle)
    {
        std::cout << "WARNING: Could not start process! \n";
        return 0;
    }

    DWORD PID = GetProcessId(pHandle);

    if (hDriver == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to get driver handle. Error: " << GetLastError() << std::endl;
    }
    else {
        DeviceIoControl(hDriver, IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH, &PID, sizeof(DWORD), NULL, NULL, NULL, NULL);
        DeviceIoControl(hDriver, IOCTL_SNAPSHOT_HASH_TEXT_SECTION, NULL, NULL, NULL, NULL, NULL, NULL);
    }

    std::string path;

    std::cout << "Enter path of game to launch: ";
    std::cin >> path;

    std::cout << "\n\n" << "---------------\n";
    std::cout << "Starting: " << path << "\n";

    bool tamperDetected = false;

    if(hDriver)DeviceIoControl(hDriver, IOCTL_VERIFY_SNAPSHOT_HASH, NULL, NULL, &tamperDetected, sizeof(bool), NULL, NULL);

    if (tamperDetected)
    {
        std::cout << "tamper!!";
    }

    std::cin >> path;

    if(hDriver) CloseHandle(hDriver);
    UnloadDriver();

    return 1;
}