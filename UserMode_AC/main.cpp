#include <iostream>
#include <string>
#include <Windows.h>
#include "main.h"

#define IOCTL_NOTIFY_DRIVER_PROCESS_TERMINATE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x20001, METHOD_BUFFERED, FILE_ANY_ACCESS)


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

        CloseHandle(hDriver);
    }


    if (!CreateNewProcess("C:/Windows/system32/notepad.exe"))
    {
        std::cout << "WARNING: Could not start process! \n";
        // goto retry;
    }


    std::string path;

retry:
    std::cout << "Enter path of game to launch: ";
    std::cin >> path;

    std::cout << "\n\n" << "---------------\n";
    std::cout << "Starting: " << path << "\n";


    UnloadDriver();

    return 1;
}