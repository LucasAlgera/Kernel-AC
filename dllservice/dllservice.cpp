#include "pch.h"
#include "framework.h"
#include "dllservice.h"

#define PIPE_NAME "\\\\.\\pipe\\AC"

DLLSERVICE_API bool NotifyMouseClicked(void)
{
    HANDLE hPipe = CreateFileA(
        PIPE_NAME,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE) 
    {
        return false;
    }

    const char* msg = "MOUSE_CLICKED";
    DWORD written;
    WriteFile(hPipe, msg, (DWORD)strlen(msg), &written, NULL);

    CloseHandle(hPipe);
    return true;
}
