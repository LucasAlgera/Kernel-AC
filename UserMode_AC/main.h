#pragma once
#include <Windows.h>
#include <vector>
#define DRIVER_NAME L"KernelAC"
#define PIPE_NAME "\\\\.\\pipe\\AC"
#define BUF_SIZE  32

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

bool CopyOwnFileTo(const std::string& fn, const std::string& d)
{
    char currentDir[MAX_PATH];

    DWORD result = GetCurrentDirectoryA(MAX_PATH, currentDir);
    if (result == 0 || result >= MAX_PATH)
        return false;

    std::string filePath = std::string(currentDir) + "\\" + fn;

    return CopyFileA(filePath.c_str(), d.c_str(), FALSE) != FALSE;
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

HANDLE hPipe;

bool CreateServer()
{
    hPipe = CreateNamedPipeA(
        PIPE_NAME,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,       
        BUF_SIZE,
        BUF_SIZE,
        0,       
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        std::cout << "Cant create Pipe.\n";
        return false;
    }
    return true;
}

bool IsMouseClicked()
{
    if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
        std::cout << "ConnectNamedPipe failed: " << GetLastError() << "\n";
        CloseHandle(hPipe);
        return false;
    }

    char buf[BUF_SIZE];
    DWORD bytesRead;
    if (ReadFile(hPipe, buf, BUF_SIZE, &bytesRead, NULL) && bytesRead > 0) 
    {
        if (bytesRead == 13 && memcmp(buf, "MOUSE_CLICKED", 13) == 0) 
        { 
            std::cout << "Mouse clicked!";
            return true;
        }
    }

    CloseHandle(hPipe);
    return false;
}

bool InjectDLL(DWORD PID, const char* dllName)
{
    HANDLE hproc = OpenProcess(PROCESS_ALL_ACCESS, false, PID);

    SIZE_T pathLen = strlen(dllName) + 1;

    void* addr = VirtualAllocEx(hproc, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (addr != nullptr)
    {
        if(!WriteProcessMemory(hproc, addr, dllName, pathLen, NULL))
        {
            VirtualFreeEx(hproc, addr, 0, MEM_RELEASE);
            return false;
        }
        FARPROC pLoadLib = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

        HANDLE htread = CreateRemoteThread(hproc, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLib, addr, 0, NULL);
        if(!htread)
        {
            VirtualFreeEx(hproc, addr, 0, MEM_RELEASE);
            return false;
        }
        WaitForSingleObject(htread, INFINITE);

        CloseHandle(htread);
        VirtualFreeEx(hproc, addr, 0, MEM_RELEASE);
        CloseHandle(hproc);

        std::cout << "DLL loaded into game memory.\n";
        return true;
    }
    return false;
}