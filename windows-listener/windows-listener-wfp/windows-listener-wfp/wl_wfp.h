#pragma once

#define NDIS684 1

#pragma warning(push)
#pragma warning(disable : 5103)

#include <ntddk.h>
#include <winerror.h>
#include <guiddef.h>
#include <initguid.h>
#include <wdm.h>
#include <fwpsk.h>
#include <fwpmu.h>

#pragma warning(pop)

typedef struct _WFPTracer *WFPTracerHandle;
