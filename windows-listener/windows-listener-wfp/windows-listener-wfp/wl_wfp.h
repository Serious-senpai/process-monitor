#pragma once

#define NDIS61 1

#include <ntddk.h>
#include <winerror.h>
#include <guiddef.h>
#include <initguid.h>
#include <wdm.h>
#include <fwpsk.h>
#include <fwpmu.h>

typedef struct _WFPTracer* WFPTracerHandle;

void free_wfp_tracer(WFPTracerHandle tracer);
WFPTracerHandle new_wfp_tracer(
    PDEVICE_OBJECT device,
    void (*callback)(UINT64 pid, SIZE_T size));
