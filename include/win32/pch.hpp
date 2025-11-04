#pragma once

#include "pch.hpp"

#define STATUS_END_OF_FILE static_cast<NTSTATUS>(0xC0000011L)

typedef struct _IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef NTSTATUS (*PFNtReadFile)(
    HANDLE FileHandle,
    HANDLE Event,
    /* PIO_APC_ROUTINE */ PVOID ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key);

typedef NTSTATUS (*PFNtWriteFile)(
    HANDLE FileHandle,
    HANDLE Event,
    /* PIO_APC_ROUTINE */ PVOID ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key);

typedef ULONG (*PFRtlNtStatusToDosError)(NTSTATUS Status);

const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
const PFNtReadFile NtReadFile = reinterpret_cast<PFNtReadFile>(GetProcAddress(ntdll, "NtReadFile"));
const PFNtWriteFile NtWriteFile = reinterpret_cast<PFNtWriteFile>(GetProcAddress(ntdll, "NtWriteFile"));
const PFRtlNtStatusToDosError RtlNtStatusToDosError =
    reinterpret_cast<PFRtlNtStatusToDosError>(GetProcAddress(ntdll, "RtlNtStatusToDosError"));

#define NT_SUCCESS(status) (static_cast<NTSTATUS>(status) >= 0)

#define OS_CVT(value_type, expr)                                                                                              \
    {                                                                                                                         \
        if ((expr) == 0)                                                                                                      \
        {                                                                                                                     \
            return io::Result<value_type>::err(io::IoError(io::IoErrorKind::Os, std::format("OS error {}", GetLastError()))); \
        }                                                                                                                     \
    }
