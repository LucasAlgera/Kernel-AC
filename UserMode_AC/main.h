#pragma once
#include <Windows.h>
#define DRIVER_NAME L"KernelAC"

HANDLE g_service = nullptr;

bool CreateNewProcess(std::string path)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    bool status = CreateProcessA(NULL, LPSTR(path.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    if (status) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return status;
}

bool InitializeDriver()
{
    WCHAR CurrentDir[MAX_PATH];

    DWORD result = GetCurrentDirectoryW(
        MAX_PATH,
        CurrentDir
    );

    if (result == 0 || result >= MAX_PATH)
        return FALSE;

    WCHAR DriverPath[MAX_PATH];

    if (swprintf_s(
        DriverPath,
        MAX_PATH,
        L"%s\\AntiCheat.sys",
        CurrentDir) < 0)
    {
        return FALSE;
    }

    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    BOOL bResult = FALSE;

    hSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (hSCManager == NULL) {
        printf("[-] OpenSCManager failed. Error: %lu\n", GetLastError());
        return FALSE;
    }

    hService = CreateServiceW(
        hSCManager,
        DRIVER_NAME,                  // Internal service name
        L"My Sample Kernel Driver",   // Display name
        SERVICE_ALL_ACCESS,           // Desired access
        SERVICE_KERNEL_DRIVER,        // Service type: kernel driver
        SERVICE_DEMAND_START,         // Start type: manual/on-demand
        SERVICE_ERROR_NORMAL,         // Error control
        DriverPath,                     // Path to .sys file
        NULL,                         // Load order group
        NULL,                         // Tag id
        NULL,                         // Dependencies
        NULL,                         // Account name (LocalSystem)
        NULL                          // Password
    );

    if (hService == NULL) 
    {
        printf("Failed to create service\n");
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) 
        {
            printf("ERROR_SERVICE_EXISTS\n");

            hService = OpenServiceW(hSCManager, DRIVER_NAME, SERVICE_START | DELETE | SERVICE_STOP);
        }
        else 
        {
            CloseServiceHandle(hSCManager);
            return FALSE;
        }
    }

    if (hService == NULL) {
        printf("Failed to get service\n");

        CloseServiceHandle(hSCManager);
        return FALSE;
    }
    
    if (!StartServiceW(hService, 0, NULL)) 
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING) 
        {
            printf("ERROR_SERVICE_ALREADY_RUNNING\n");

            bResult = TRUE;
        }
        if (err == ERROR_INVALID_IMAGE_HASH)
        {
            printf("ERROR_INVALID_IMAGE_HASH\n");
            bResult = FALSE;
        }
    }
    else {
        bResult = TRUE;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return bResult;
}

void UnloadDriver()
{
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    SERVICE_STATUS status{ 0 };

    hSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (hSCManager == NULL) {
        printf("[-] OpenSCManager failed. Error: %lu\n", GetLastError());
        return;
    }

    hService = OpenServiceW(hSCManager, DRIVER_NAME, SERVICE_START | DELETE | SERVICE_STOP);
    if (!hService)
    {
        printf("[-] Failed to open service. Error: %lu\n", GetLastError());
        return;
    }

    if (!ControlService(hService, SERVICE_CONTROL_STOP, &status))
    {
        printf("ControlService failed: %lu\n", GetLastError());
        return;
    }

    printf("Initial state: %lu\n", status.dwCurrentState);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}