#include <iostream>
#include <string>
#include <Windows.h>
#include "main.h"

#include <chrono>
#include <thread>

#define IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20001, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NOTIFY_DRIVER_PROCESS_LAUNCH		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20002, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SNAPSHOT_HASH_TEXT_SECTION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20003, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VERIFY_SNAPSHOT_HASH				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20004, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WALK_PROCESS_LIST 				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20005, METHOD_BUFFERED, FILE_ANY_ACCESS)


int main()
{
    if (!InitializeDriver())
    {
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

    bool tamperDetected = false;
    bool blacklistProcPresent = false;;

    if(hDriver)DeviceIoControl(hDriver, IOCTL_WALK_PROCESS_LIST, NULL, NULL, NULL, NULL, NULL, NULL);
    if(hDriver)DeviceIoControl(hDriver, IOCTL_VERIFY_SNAPSHOT_HASH, NULL, NULL, &tamperDetected, sizeof(bool), NULL, NULL);

    if (tamperDetected)
    {
        std::cout << "tamper!!";
    }

    //std::srand(std::time(0));

    using namespace std::chrono_literals;

    if (hDriver)
    {
        bool tamperDetected = false;

        DeviceIoControl(hDriver, IOCTL_VERIFY_SNAPSHOT_HASH, NULL, 0, &tamperDetected, sizeof(tamperDetected), NULL, NULL);

        if (tamperDetected)
        {
            std::cout << "tamper!!\n";
        }
    }

    std::string x;
    std::cin >> x;

    if(hDriver) CloseHandle(hDriver);
    UnloadDriver();

    return 1;
}