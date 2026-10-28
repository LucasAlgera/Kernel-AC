#pragma once
#include "ntifs.h"
#include "common.h"

NTSTATUS HandleProcessLaunch(IRP* Irp);

NTSTATUS WalkProcessList(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS ScanManuallyMappedCode(DEVICE_OBJECT* DeviceObject, IRP* Irp);

NTSTATUS ScanThreadsForHWBP(DEVICE_OBJECT* DeviceObject, IRP* Irp);

VOID InspectThreadContext(HANDLE ThreadId);

VOID ParseDebugRegisters(PCONTEXT Context, HANDLE ThreadId);

// Import undocumented function(s)

EXTERN_C NTSYSAPI NTSTATUS ZwQuerySystemInformation(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

typedef NTSTATUS(NTAPI* PFN_PsGetContextThread)(
    IN PETHREAD Thread,
    IN OUT PCONTEXT ThreadContext,
    IN KPROCESSOR_MODE PreviousMode
    );

PFN_PsGetContextThread GetPsGetThreadContext();

// This is for the SYSTEM_PROCESS_INFORMATION struct. Other system information classes: 
// https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/ntexapi/system_information_class.htm
#define SystemProcessInformation 0x5


//
// View the following undocumented structures + info on: 
// https://ntdoc.m417z.com/system_process_information
// https://ntdoc.m417z.com/system_thread_information
// https://ntdoc.m417z.com/kthread_state
//

typedef enum _KTHREAD_STATE
{
    Initialized,
    Ready,
    Running,
    Standby,
    Terminated,
    Waiting,
    Transition,
    DeferredReady,
    GateWaitObsolete,
    WaitingForProcessInSwap,
    MaximumThreadState
} KTHREAD_STATE, * PKTHREAD_STATE;

typedef struct _SYSTEM_THREAD_INFORMATION
{
    LARGE_INTEGER KernelTime;                   // Number of 100-nanosecond intervals spent executing kernel code.
    LARGE_INTEGER UserTime;                     // Number of 100-nanosecond intervals spent executing user code.
    LARGE_INTEGER CreateTime;                   // The date and time when the thread was created.
    ULONG WaitTime;                             // The current time spent in ready queue or waiting (depending on the thread state).
    PVOID StartAddress;                         // The initial start address of the thread.
    CLIENT_ID ClientId;                         // The identifier of the thread and the process owning the thread.
    KPRIORITY Priority;                         // The dynamic priority of the thread.
    KPRIORITY BasePriority;                     // The starting priority of the thread.
    ULONG ContextSwitches;                      // The total number of context switches performed.
    KTHREAD_STATE ThreadState;                  // The current state of the thread.
    KWAIT_REASON WaitReason;                    // The current reason the thread is waiting.
} SYSTEM_THREAD_INFORMATION, * PSYSTEM_THREAD_INFORMATION;

typedef struct _SYSTEM_PROCESS_INFORMATION
{
    ULONG NextEntryOffset;                      // The address of the previous item plus the value in the NextEntryOffset member. For the last item in the array, NextEntryOffset is 0.
    ULONG NumberOfThreads;                      // The NumberOfThreads member contains the number of threads in the process.
    ULONGLONG WorkingSetPrivateSize;            // The total private memory that a process currently has allocated and is physically resident in memory. // since VISTA
    ULONG HardFaultCount;                       // The total number of hard faults for data from disk rather than from in-memory pages. // since WIN7
    ULONG NumberOfThreadsHighWatermark;         // The peak number of threads that were running at any given point in time, indicative of potential performance bottlenecks related to thread management.
    ULONGLONG CycleTime;                        // The sum of the cycle time of all threads in the process.
    LARGE_INTEGER CreateTime;                   // Number of 100-nanosecond intervals since the creation time of the process. Not updated during system timezone changes.
    LARGE_INTEGER UserTime;                     // Number of 100-nanosecond intervals the process has executed in user mode.
    LARGE_INTEGER KernelTime;                   // Number of 100-nanosecond intervals the process has executed in kernel mode.
    UNICODE_STRING ImageName;                   // The file name of the executable image.
    KPRIORITY BasePriority;                     // The starting priority of the process.
    HANDLE UniqueProcessId;                     // The identifier of the process.
    HANDLE InheritedFromUniqueProcessId;        // The identifier of the process that created this process. Not updated and incorrectly refers to processes with recycled identifiers.
    ULONG HandleCount;                          // The current number of open handles used by the process.
    ULONG SessionId;                            // The identifier of the Remote Desktop Services session under which the specified process is running.
    ULONG_PTR UniqueProcessKey;                 // since VISTA (requires SystemExtendedProcessInformation)
    SIZE_T PeakVirtualSize;                     // The peak size, in bytes, of the virtual memory used by the process.
    SIZE_T VirtualSize;                         // The current size, in bytes, of virtual memory used by the process.
    ULONG PageFaultCount;                       // The total number of page faults for data that is not currently in memory. The value wraps around to zero on average 24 hours.
    SIZE_T PeakWorkingSetSize;                  // The peak size, in kilobytes, of the working set of the process.
    SIZE_T WorkingSetSize;                      // The number of pages visible to the process in physical memory. These pages are resident and available for use without triggering a page fault.
    SIZE_T QuotaPeakPagedPoolUsage;             // The peak quota charged to the process for pool usage, in bytes.
    SIZE_T QuotaPagedPoolUsage;                 // The quota charged to the process for paged pool usage, in bytes.
    SIZE_T QuotaPeakNonPagedPoolUsage;          // The peak quota charged to the process for nonpaged pool usage, in bytes.
    SIZE_T QuotaNonPagedPoolUsage;              // The current quota charged to the process for nonpaged pool usage.
    SIZE_T PagefileUsage;                       // The total number of bytes of page file storage in use by the process.
    SIZE_T PeakPagefileUsage;                   // The maximum number of bytes of page-file storage used by the process.
    SIZE_T PrivatePageCount;                    // The number of memory pages allocated for the use by the process.
    LARGE_INTEGER ReadOperationCount;           // The total number of read operations performed.
    LARGE_INTEGER WriteOperationCount;          // The total number of write operations performed.
    LARGE_INTEGER OtherOperationCount;          // The total number of I/O operations performed other than read and write operations.
    LARGE_INTEGER ReadTransferCount;            // The total number of bytes read during a read operation.
    LARGE_INTEGER WriteTransferCount;           // The total number of bytes written during a write operation.
    LARGE_INTEGER OtherTransferCount;           // The total number of bytes transferred during operations other than read and write operations.
    SYSTEM_THREAD_INFORMATION Threads[1];       // This type is not defined in the structure but was added for convenience.
} SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;