#define NDIS61 1

#include <ntddk.h>
#include <winerror.h>
#include <guiddef.h>
#include <initguid.h>
#include <wdm.h>
#include <fwpsk.h>
#include <fwpmu.h>

#define LOG(format, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Windows Listener WFP] " format, ##__VA_ARGS__)

#define ON_DWERROR(dword, on_error, message, ...)           \
    {                                                       \
        DWORD status = (dword);                             \
        if (status != ERROR_SUCCESS)                        \
        {                                                   \
            LOG(message ": 0x%08X", ##__VA_ARGS__, status); \
            on_error;                                       \
        }                                                   \
    }

#define INSPECT_DWERROR(dword, message, ...) ON_DWERROR(dword, { }, message, ##__VA_ARGS__)

#define ON_NTERROR(ntstatus, on_error, message, ...)        \
    {                                                       \
        DWORD status = (ntstatus);                          \
        if (!NT_SUCCESS(status))                            \
        {                                                   \
            LOG(message ": 0x%08X", ##__VA_ARGS__, status); \
            on_error;                                       \
        }                                                   \
    }

#define INSPECT_NTERROR(ntstatus, message, ...) ON_NTERROR(ntstatus, { }, message, ##__VA_ARGS__)

DEFINE_GUID(
    WINDOWS_LISTENER_WFP_SUBLAYER,
    0xf1d1437a, 0x4389, 0x4866, 0x91, 0x49, 0xf0, 0x4a, 0xa4, 0x38, 0x81, 0x0d);

DEFINE_GUID(WINDOWS_LISTENER_GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

PDRIVER_OBJECT DRIVER = NULL;
HANDLE FILTER_ENGINE = NULL;

typedef struct _RegisteredCallout
{
    UINT32 fwps_callout_id;
	GUID fwpm_callout_key;
	UINT64 fwpm_filter_id;
} RegisteredCallout;

RegisteredCallout ALE_V4 = { 0 }, ALE_V6 = { 0 }, TCP_STREAM_V4 = { 0 }, TCP_STREAM_V6 = { 0 };

static NTSTATUS register_callout(
    IN PDEVICE_OBJECT device,
    IN const FWPS_CALLOUT0* callout,
    IN const GUID *fwpm_applicable_layer,
	IN UINT16 fwps_target_layer_id,
	IN UINT32 fwps_target_callout_id,
	IN const FWPM_SUBLAYER0* sublayer,
    OUT RegisteredCallout *result)
{
    if (callout == NULL || sublayer == NULL || result == NULL)
    {
		LOG("At least 1 parameter is NULL");
		return STATUS_INVALID_PARAMETER;
    }

	HANDLE filter_engine = FILTER_ENGINE;
    if (filter_engine == NULL)
    {
        LOG("Filter engine has not been initialized yet.");
		return STATUS_UNSUCCESSFUL;
    }

    UINT32 fwps_callout_id = 0;
    NTSTATUS status = FwpsCalloutRegister0(device, callout, &fwps_callout_id);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register callout: 0x%08X", status);
        return status;
    }

    FWPM_CALLOUT0 mcallout = { 0 };
    mcallout.calloutKey = callout->calloutKey;
    mcallout.displayData.name = L"Windows Listener WFP Callout";
    mcallout.displayData.description = L"Monitors network traffic";
    mcallout.applicableLayer = *fwpm_applicable_layer;

    DWORD ustatus = FwpmCalloutAdd0(filter_engine, &mcallout, NULL, NULL);
    if (ustatus != ERROR_SUCCESS && ustatus != ERROR_ALREADY_EXISTS)
    {
		LOG("Failed to add callout to filter engine: 0x%08X", ustatus);
        return STATUS_UNSUCCESSFUL;
    }

    FWPM_FILTER0 filter = { 0 };
    filter.displayData.name = L"Windows Listener WFP filter";
    filter.flags = FWPM_FILTER_FLAG_NONE;
    filter.layerKey = *fwpm_applicable_layer;
    filter.subLayerKey = WINDOWS_LISTENER_WFP_SUBLAYER;
    filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
    filter.action.calloutKey = callout->calloutKey;
    filter.rawContext = ((UINT64)fwps_target_layer_id << 32) | (UINT64)fwps_target_callout_id;

    UINT64 fwpm_filter_id = 0;
    ustatus = FwpmFilterAdd0(filter_engine, &filter, NULL, &fwpm_filter_id);
    if (ustatus != ERROR_SUCCESS)
    {
        LOG("Failed to add filter to filter engine: 0x%08X", ustatus);
        return STATUS_UNSUCCESSFUL;
    }

	result->fwps_callout_id = fwps_callout_id;
	result->fwpm_callout_key = callout->calloutKey;
	result->fwpm_filter_id = fwpm_filter_id;
	return STATUS_SUCCESS;
}

static BOOL unregister_callout(IN OUT RegisteredCallout* callout)
{
	BOOL success = TRUE;
    if (FILTER_ENGINE != NULL)
    {
        if (callout->fwpm_filter_id != 0)
        {
            ON_DWERROR(
                FwpmFilterDeleteById0(FILTER_ENGINE, callout->fwpm_filter_id),
                success = FALSE,
                "Cannot remove filter from filter engine");
            callout->fwpm_filter_id = 0;
        }

        if (IsEqualGUID(&callout->fwpm_callout_key, &WINDOWS_LISTENER_GUID_NULL))
        {
            ON_DWERROR(
                FwpmCalloutDeleteByKey0(FILTER_ENGINE, &callout->fwpm_callout_key),
                success = FALSE,
                "Cannot remove callout from filter engine");
			callout->fwpm_callout_key = WINDOWS_LISTENER_GUID_NULL;
        }
    }

    if (callout->fwps_callout_id != 0)
    {
        ON_NTERROR(
            FwpsCalloutUnregisterById0(callout->fwps_callout_id),
			success = FALSE,
            "Cannot unregister callout"); // FIXME: If a flow context is still associated, this will return STATUS_DEVICE_BUSY
        callout->fwps_callout_id = 0;
    }

    return success;
}

static VOID NTAPI ale_classify(
    IN const FWPS_INCOMING_VALUES0 *in_fixed_values,
    IN const FWPS_INCOMING_METADATA_VALUES0 *in_meta_values,
    IN OUT VOID *layer_data,
    IN const FWPS_FILTER0 *filter,
    IN UINT64 flow_context,
    IN OUT FWPS_CLASSIFY_OUT0 *classify_out)
{
    UNREFERENCED_PARAMETER(layer_data);
    classify_out->actionType = FWP_ACTION_PERMIT;

    if (in_fixed_values != NULL &&
        in_meta_values != NULL &&
        FWPS_IS_METADATA_FIELD_PRESENT(in_meta_values, FWPS_METADATA_FIELD_PROCESS_ID) &&
        FWPS_IS_METADATA_FIELD_PRESENT(in_meta_values, FWPS_METADATA_FIELD_FLOW_HANDLE) &&
        flow_context == 0)
    {
        UINT64 pid = in_meta_values->processId;

        // In order to use `FwpsFlowAssociateContext0`, we need to define a `flow_delete` function
        // below (even a dummy one works), or else this call will return `STATUS_INVALID_PARAMETER`.
        NTSTATUS status = FwpsFlowAssociateContext0(
            in_meta_values->flowHandle,
            (UINT16)(filter->context >> 32),
            (UINT32)(filter->context & 0xFFFFFFFF),
            pid // In order to pass more complex data, allocate a struct and free it in `flow_delete`
        );
        if (!NT_SUCCESS(status))
        {
            LOG("Warning: Failed to associate flow context to layer: 0x%08X", status);
        }
    }
}

static VOID NTAPI tcp_stream_classify(
    IN const FWPS_INCOMING_VALUES0 *in_fixed_values,
    IN const FWPS_INCOMING_METADATA_VALUES0 *in_meta_values,
    IN OUT VOID *layer_data,
    IN const FWPS_FILTER0 *filter,
    IN UINT64 flow_context,
    IN OUT FWPS_CLASSIFY_OUT0 *classify_out)
{
    UNREFERENCED_PARAMETER(in_fixed_values);
    UNREFERENCED_PARAMETER(in_meta_values);
    UNREFERENCED_PARAMETER(filter);
    classify_out->actionType = FWP_ACTION_PERMIT;

    UINT64 pid = flow_context;
    if (pid != 0 && layer_data != NULL)
    {
        const FWPS_STREAM_CALLOUT_IO_PACKET0 *data = (const FWPS_STREAM_CALLOUT_IO_PACKET0 *)layer_data;
        const FWPS_STREAM_DATA0 *stream = data->streamData;
        if (stream->dataLength > 0)
        {
            LOG("TCP traffic: PID %llu (%Iu bytes)", pid, stream->dataLength);
        }
    }
}

static NTSTATUS NTAPI notify(
    IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
    IN const GUID *filterKey,
    IN const FWPS_FILTER0 *filter)
{
    UNREFERENCED_PARAMETER(notifyType);
    UNREFERENCED_PARAMETER(filterKey);
    UNREFERENCED_PARAMETER(filter);
    return STATUS_SUCCESS;
}

static VOID NTAPI flow_delete(
    IN UINT16 layerId,
    IN UINT32 calloutId,
    IN UINT64 flowContext)
{
    UNREFERENCED_PARAMETER(layerId);
    UNREFERENCED_PARAMETER(calloutId);
    UNREFERENCED_PARAMETER(flowContext);
}

const FWPS_CALLOUT0 ALE_V4_CALLOUT = {
    {0xda4d8b9a, 0x7d13, 0x4c89, {0x9b, 0x3c, 0xb4, 0x44, 0x73, 0x18, 0x2f, 0x93}},
    0,
    ale_classify,
    notify,
    flow_delete};

const FWPS_CALLOUT0 ALE_V6_CALLOUT = {
    {0xda4d8b9a, 0x7d13, 0x4c89, {0x9b, 0x3c, 0xb4, 0x44, 0x73, 0x18, 0x2f, 0x94}},
    0,
    ale_classify,
    notify,
    flow_delete };

const FWPS_CALLOUT0 TCP_STREAM_V4_CALLOUT = {
    {0x37d2ded2, 0xce93, 0x4e2a, {0xb6, 0x77, 0xd8, 0x0c, 0x73, 0x4f, 0x70, 0x95}},
    0,
    tcp_stream_classify,
    notify,
    flow_delete};

const FWPS_CALLOUT0 TCP_STREAM_V6_CALLOUT = {
    {0x37d2ded2, 0xce93, 0x4e2a, {0xb6, 0x77, 0xd8, 0x0c, 0x73, 0x4f, 0x70, 0x96}},
    0,
    tcp_stream_classify,
    notify,
    flow_delete };

static VOID driver_unload(PDRIVER_OBJECT driver)
{
    LOG("driver_unload");
    DRIVER = NULL;

    if (FILTER_ENGINE != NULL)
    {
        if (!unregister_callout(&ALE_V6))
        {
			LOG("Failed to unregister ALE_V6 callout");
        }
        
        if (!unregister_callout(&ALE_V4))
        {
            LOG("Failed to unregister ALE_V4 callout");
		}

        if (!unregister_callout(&TCP_STREAM_V6))
        {
            LOG("Failed to unregister TCP_STREAM_V6 callout");
		}

        if (!unregister_callout(&TCP_STREAM_V4))
        {
            LOG("Failed to unregister TCP_STREAM_V4 callout");
		}

        INSPECT_DWERROR(
            FwpmSubLayerDeleteByKey0(FILTER_ENGINE, &WINDOWS_LISTENER_WFP_SUBLAYER),
            "Cannot remove sublayer WINDOWS_LISTENER_WFP_SUBLAYER");
        INSPECT_DWERROR(
            FwpmEngineClose0(FILTER_ENGINE),
            "Cannot close WFP engine");
        FILTER_ENGINE = NULL;
    }

    PDEVICE_OBJECT device = driver->DeviceObject;
    if (device != NULL)
    {
        IoDeleteDevice(device);
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    UNREFERENCED_PARAMETER(registry);
    LOG("DriverEntry");
    driver->DriverUnload = driver_unload;

    NTSTATUS status = STATUS_SUCCESS;

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
        LOG("Failed to create device: 0x%08X", status);
        driver_unload(driver);
        return status;
    }

    DWORD ustatus = FwpmEngineOpen0(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &FILTER_ENGINE);
    if (ustatus != ERROR_SUCCESS)
    {
        LOG("Failed to open filter engine: 0x%08X", ustatus);
        driver_unload(driver);
        return STATUS_UNSUCCESSFUL;
    }

    FWPM_SUBLAYER0 sublayer = { 0 };
    sublayer.subLayerKey = WINDOWS_LISTENER_WFP_SUBLAYER;
    sublayer.displayData.name = L"Windows Listener WFP Sublayer";
    sublayer.displayData.description = L"Sublayer for Windows Listener WFP Callout driver";
    sublayer.weight = 0x100;

    ustatus = FwpmSubLayerAdd0(FILTER_ENGINE, &sublayer, NULL);
    if (ustatus != ERROR_SUCCESS && ustatus != ERROR_ALREADY_EXISTS)
    {
        LOG("Failed to add sublayer: 0x%08X", ustatus);
        driver_unload(driver);
        return STATUS_UNSUCCESSFUL;
    }

    status = register_callout(
        device,
        &TCP_STREAM_V4_CALLOUT,
        &FWPM_LAYER_STREAM_V4,
        0,
        0,
        &sublayer,
        &TCP_STREAM_V4);
    if (!NT_SUCCESS(status))
    {
		LOG("Failed to register TCP_STREAM_V4 callout: 0x%08X", status);
        driver_unload(driver);
		return status;
    }

    status = register_callout(
        device,
        &TCP_STREAM_V6_CALLOUT,
        &FWPM_LAYER_STREAM_V6,
        0,
        0,
        &sublayer,
        &TCP_STREAM_V6);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register TCP_STREAM_V6 callout: 0x%08X", status);
        driver_unload(driver);
        return status;
    }

    status = register_callout(
        device,
        &ALE_V4_CALLOUT,
        &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,
        FWPS_LAYER_STREAM_V4,
        TCP_STREAM_V4.fwps_callout_id,
        &sublayer,
        &ALE_V4);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register ALE_V4 callout: 0x%08X", status);
        driver_unload(driver);
        return status;
    }

    status = register_callout(
        device,
        &ALE_V6_CALLOUT,
        &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6,
        FWPS_LAYER_STREAM_V6,
        TCP_STREAM_V6.fwps_callout_id,
        &sublayer,
        &ALE_V6);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register ALE_V6 callout: 0x%08X", status);
        driver_unload(driver);
        return status;
    }

    DRIVER = driver;
    return status;
}
