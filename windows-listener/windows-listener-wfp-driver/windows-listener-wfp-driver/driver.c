#define NDIS60 1

#include <ntddk.h>
#include <wdm.h>
#include <fwpsk.h>

PDRIVER_OBJECT DRIVER = NULL;
UINT32 CALLOUT_ID = 0;

VOID NTAPI classify(
    IN const FWPS_INCOMING_VALUES0 *inFixedValues,
    IN const FWPS_INCOMING_METADATA_VALUES0 *inMetaValues,
    IN OUT VOID *layerData,
    IN const FWPS_FILTER0 *filter,
    IN UINT64 flowContext,
    IN OUT FWPS_CLASSIFY_OUT0 *classifyOut)
{
    UNREFERENCED_PARAMETER(inFixedValues);
    UNREFERENCED_PARAMETER(layerData);
    UNREFERENCED_PARAMETER(filter);
    UNREFERENCED_PARAMETER(flowContext);
    UNREFERENCED_PARAMETER(classifyOut);

    UINT32 pid = 0;

    //
    // 1) Get PID (if available for this layer)
    //
    if (inMetaValues != NULL &&
        (inMetaValues->currentMetadataValues & FWPS_METADATA_FIELD_PROCESS_ID))
    {
        pid = (UINT32)(UINT_PTR)inMetaValues->processId;
    }

    //
    // 2) Count bytes from NET_BUFFER_LIST (layerData)
    //
    //if (layerData)
    //{
    //    NET_BUFFER_LIST* nbl = (NET_BUFFER_LIST*)layerData;
    //    NET_BUFFER* nb = NET_BUFFER_LIST_FIRST_NB(nbl);

    //    while (nb)
    //    {
    //        bytes += NET_BUFFER_DATA_LENGTH(nb);
    //        nb = NET_BUFFER_NEXT_NB(nb);
    //    }
    //}

    //
    // 3) Print to DebugView WITHOUT verbose mode
    //
    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[WFP] PID=%u, Bytes=...\n",
        pid
    );
}

NTSTATUS NTAPI notify(
    IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
    IN const GUID *filterKey,
    IN const FWPS_FILTER0 *filter)
{
    UNREFERENCED_PARAMETER(notifyType);
    UNREFERENCED_PARAMETER(filterKey);
    UNREFERENCED_PARAMETER(filter);
    return STATUS_SUCCESS;
}

VOID NTAPI flow_delete(
    IN UINT16 layerId,
    IN UINT32 calloutId,
    IN UINT64 flowContext)
{
    UNREFERENCED_PARAMETER(layerId);
    UNREFERENCED_PARAMETER(calloutId);
    UNREFERENCED_PARAMETER(flowContext);
}

const FWPS_CALLOUT0 CALLOUT = {
    {0xda4d8b9a, 0x7d13, 0x4c89, {0x9b, 0x3c, 0xb4, 0x44, 0x73, 0x18, 0x2f, 0x93}},
    0,
    classify,
    notify,
    flow_delete};

VOID unload(PDRIVER_OBJECT driver)
{
    PDEVICE_OBJECT device = driver->DeviceObject;
    if (device != NULL)
    {
        IoDeleteDevice(device);
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    UNREFERENCED_PARAMETER(registry);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[WFP] DriverEntry\n");
    NTSTATUS status = STATUS_SUCCESS;
    driver->DriverUnload = unload;

    PDEVICE_OBJECT device = NULL;
    status = IoCreateDevice(
        driver,
        0,
        NULL,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &device);
    if (!NT_SUCCESS(status))
    {
        unload(driver);
        return status;
    }

    status = FwpsCalloutRegister0(device, &CALLOUT, &CALLOUT_ID);
    if (!NT_SUCCESS(status))
    {
        unload(driver);
        return status;
    }

    DRIVER = driver;
    return status;
}
