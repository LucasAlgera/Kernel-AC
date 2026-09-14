#include "hashing.h"
#include <bcrypt.h>
#include "common.h"
#include <ntifs.h>
#include <ntstrsafe.h>

#define CODE_LENGTH 1024
#define SECTION_HEADER_LENGTH 0x28

NTSTATUS ComputeSHA256(
    _In_reads_bytes_(inputlen) const unsigned char* input,
    _In_ ULONG inputlen,
    _Out_writes_bytes_(32) unsigned char* hash
)
{
    BCRYPT_ALG_HANDLE hAlgorithm = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    PUCHAR hashObject = NULL;

    ULONG objectLength = 0;
    ULONG hashLength = 0;
    ULONG resultLength = 0;

    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(
        &hAlgorithm,
        BCRYPT_SHA256_ALGORITHM,
        NULL,
        0
    );

    if (!NT_SUCCESS(status))
        goto cleanup;

    status = BCryptGetProperty(
        hAlgorithm,
        BCRYPT_OBJECT_LENGTH,
        (PUCHAR)&objectLength,
        sizeof(objectLength),
        &resultLength,
        0
    );

    if (!NT_SUCCESS(status))
        goto cleanup;

    status = BCryptGetProperty(
        hAlgorithm,
        BCRYPT_HASH_LENGTH,
        (PUCHAR)&hashLength,
        sizeof(hashLength),
        &resultLength,
        0
    );

    if (!NT_SUCCESS(status))
        goto cleanup;

    if (hashLength != 32)
    {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto cleanup;
    }

    hashObject = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, objectLength, 'hsah');

    if (hashObject == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    status = BCryptCreateHash(hAlgorithm, &hHash, hashObject, objectLength, NULL, 0, 0);

    if (!NT_SUCCESS(status))
        goto cleanup;

    status = BCryptHashData(hHash, (PUCHAR)input, inputlen, 0);

    if (!NT_SUCCESS(status))
        goto cleanup;

    status = BCryptFinishHash(hHash, hash, hashLength, 0);

cleanup:

    if (hHash != NULL)
        BCryptDestroyHash(hHash);
    if (hashObject != NULL)
        ExFreePoolWithTag(hashObject, 'hsah');
    if (hAlgorithm != NULL)
        BCryptCloseAlgorithmProvider(hAlgorithm, 0);
    return status;
}

static void PrintSHA256(_In_reads_bytes_(32) unsigned char* hash)
{
    char hexString[65]; // 32*2 + null terminator

    for (int i = 0; i < 32; i++)
    {
        RtlStringCchPrintfA(&hexString[i * 2], 65 - (i * 2), "%02x", hash[i]);
    }

    DbgPrint("Hash: %s\n", hexString);
}

NTSTATUS GetTextSectionFromMonitoredProcess(PUCHAR* code, uintptr_t offset)
{
	PEPROCESS process;
    uintptr_t procbase = 0, codeBase = 0;
    ULONG text_size;

	if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)g_DriverExtention->PID, &process)))
		return STATUS_UNSUCCESSFUL;

	procbase = (uintptr_t)PsGetProcessSectionBaseAddress(process);

    uintptr_t addr; // address jumping through the PE
    int exeHdr;
    USHORT sections, sOptHdr;

    // This is pretty scuffed, might eventually use windows structures. But eh

    exeHdr = *(int*)(procbase + 0x3C);  // File address of new exe header
    addr = procbase + exeHdr;           // PE magic
    sections = *(USHORT*)(addr + 0x6);  // Amount of sections
    sOptHdr = *(USHORT*)(addr + 0x14);  // Size of OptionalHeader
    addr = addr + 0x18;                 // Optional header
    addr = addr + sOptHdr;              // Section header

    for (USHORT i = 0; i < sections; i++)
    {
        if (RtlCompareMemory((PVOID)addr, ".text", 5) == 5)
        {
            codeBase = procbase + *(DWORD*)(addr + 0x0C);
            text_size = *(ULONG*)(addr + 0x08);

            if (offset + CODE_LENGTH > text_size)
                break;

            codeBase += offset;

            break;
        }
        addr += SECTION_HEADER_LENGTH;
    }

    if ((PVOID)codeBase != NULL)
    {
	    memcpy(*code, (PVOID)codeBase, CODE_LENGTH); // bit unsafe might resort to MmCopyVirtualMemory
    }

    ObDereferenceObject(process);

	return STATUS_SUCCESS;
}



NTSTATUS TakeHashSnapshot(DEVICE_OBJECT* DeviceObject, IRP* Irp)
{
	PUCHAR code = NULL;
	PEPROCESS process;
	KAPC_STATE ApcState;

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	// Use KeStackAttachProcess to attach to the user-mode space
	PsLookupProcessByProcessId((HANDLE)g_DriverExtention->PID, &process);
	KeStackAttachProcess((PRKPROCESS)process, &ApcState);


	code = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, CODE_LENGTH, 'edoc');


	if (!NT_SUCCESS(GetTextSectionFromMonitoredProcess(&code, 0x20)))
	{
		DbgPrint("Fail..");
		return STATUS_UNSUCCESSFUL;
	}

	__try
	{
		if (code != NULL)
		{
			unsigned char hash[32];

			ComputeSHA256(code, CODE_LENGTH, hash);
            PrintSHA256(hash);
            DbgPrint("First byte: %02x", code[0]);
		}

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		DbgPrint("FAIL!");
	}

	KeUnstackDetachProcess(&ApcState); // go back to own address space
	if (code) ExFreePoolWithTag(code, 'edoc');

	return STATUS_SUCCESS;
}
