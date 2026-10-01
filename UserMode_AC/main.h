#pragma once
#include <Windows.h>
#include <vector>
#define DRIVER_NAME L"KernelAC"

HANDLE g_service = nullptr;

HANDLE CreateNewProcess(std::string path)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    bool status = CreateProcessA(NULL, LPSTR(path.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    /*if (status) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }*/
    return pi.hProcess;
}

bool CopyDriverToFolder()
{
    WCHAR CurrentDir[MAX_PATH];
    DWORD result = GetCurrentDirectoryW(MAX_PATH, CurrentDir);

    if (result == 0 || result >= MAX_PATH)
        return FALSE;

    WCHAR DriverPath[MAX_PATH];
    if (swprintf_s(DriverPath, MAX_PATH, L"%s\\AntiCheat.sys", CurrentDir) < 0)
        return FALSE;

    WCHAR ExpandedRoot[MAX_PATH];
    if (ExpandEnvironmentStringsW(L"%SystemRoot%", ExpandedRoot, MAX_PATH) == 0)
        return FALSE;

    WCHAR DestPath[MAX_PATH];
    if (swprintf_s(DestPath, MAX_PATH, L"%s\\System32\\drivers\\AntiCheat.sys", ExpandedRoot) < 0)
        return FALSE;

    if (!CopyFileW(DriverPath, DestPath, FALSE))
        return FALSE;

    return TRUE;
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
        SERVICE_AUTO_START,         // Start type: manual/on-demand
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

// This function was done with Claude
// It pretty much inserts "KernelAC" in the registry right before "mouclass"
bool InitializeFilterDriver()
{
    HKEY hKey;
    const wchar_t* subKey =
        L"SYSTEM\\CurrentControlSet\\Control\\Class\\"
        L"{4D36E96F-E325-11CE-BFC1-08002BE10318}";

    const wchar_t* serviceName = L"KernelAC";

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        subKey,
        0,
        KEY_READ | KEY_WRITE,
        &hKey) != ERROR_SUCCESS)
    {
        return false;
    }

    DWORD size = 0;
    DWORD type = 0;

    if (RegQueryValueExW(
        hKey,
        L"UpperFilters",
        nullptr,
        &type,
        nullptr,
        &size) != ERROR_SUCCESS ||
        type != REG_MULTI_SZ)
    {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> buffer(size / sizeof(wchar_t));

    if (RegQueryValueExW(
        hKey,
        L"UpperFilters",
        nullptr,
        &type,
        reinterpret_cast<LPBYTE>(buffer.data()),
        &size) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return false;
    }

    // Check whether KernelAC already exists.
    for (wchar_t* p = buffer.data(); *p; p += wcslen(p) + 1)
    {
        if (_wcsicmp(p, serviceName) == 0)
        {
            RegCloseKey(hKey);
            return true;
        }
    }

    // Build a new REG_MULTI_SZ.
    std::vector<wchar_t> newBuffer;

    bool inserted = false;

    for (wchar_t* p = buffer.data(); *p; p += wcslen(p) + 1)
    {
        // Insert KernelAC immediately before mouclass.
        if (!inserted && _wcsicmp(p, L"mouclass") == 0)
        {
            newBuffer.insert(
                newBuffer.end(),
                serviceName,
                serviceName + wcslen(serviceName) + 1);

            inserted = true;
        }

        // Copy the existing entry.
        newBuffer.insert(
            newBuffer.end(),
            p,
            p + wcslen(p) + 1);
    }

    // If mouclass wasn't found, append KernelAC at the end.
    if (!inserted)
    {
        newBuffer.insert(
            newBuffer.end(),
            serviceName,
            serviceName + wcslen(serviceName) + 1);
    }

    // REG_MULTI_SZ requires an additional terminating NULL.
    newBuffer.push_back(L'\0');

    DWORD outSize =
        static_cast<DWORD>(newBuffer.size() * sizeof(wchar_t));

    LONG result = RegSetValueExW(
        hKey,
        L"UpperFilters",
        0,
        REG_MULTI_SZ,
        reinterpret_cast<const BYTE*>(newBuffer.data()),
        outSize);

    RegCloseKey(hKey);

    return result == ERROR_SUCCESS;
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