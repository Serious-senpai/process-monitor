// WFP layer identifiers: https://learn.microsoft.com/en-us/windows/win32/fwp/management-filtering-layer-identifiers-

#include "wl_wfp.h"

#define LOG(format, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Windows Listener WFP] " format "\n" __VA_OPT__(, ) __VA_ARGS__)

#define ON_DWERROR(dword, on_error, message, ...)                       \
    {                                                                   \
        DWORD status = (dword);                                         \
        if (status != ERROR_SUCCESS)                                    \
        {                                                               \
            LOG(message ": 0x%08X" __VA_OPT__(, ) __VA_ARGS__, status); \
            on_error;                                                   \
        }                                                               \
    }

#define INSPECT_DWERROR(dword, message, ...) ON_DWERROR(dword, {}, message __VA_OPT__(, ) __VA_ARGS__)

#define ON_NTERROR(ntstatus, on_error, message, ...)                    \
    {                                                                   \
        DWORD status = (ntstatus);                                      \
        if (!NT_SUCCESS(status))                                        \
        {                                                               \
            LOG(message ": 0x%08X" __VA_OPT__(, ) __VA_ARGS__, status); \
            on_error;                                                   \
        }                                                               \
    }

#define INSPECT_NTERROR(ntstatus, message, ...) ON_NTERROR(ntstatus, {}, message __VA_OPT__(, ) __VA_ARGS__)

#define POOL_TAG 'PFWL'

DEFINE_GUID(
    WINDOWS_LISTENER_WFP_SUBLAYER,
    0xf1d1437a, 0x4389, 0x4866, 0x91, 0x49, 0xf0, 0x4a, 0xa4, 0x38, 0x81, 0x0d);

DEFINE_GUID(WINDOWS_LISTENER_GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

typedef struct _FlowContext
{
    // Entry in the global linked list
    LIST_ENTRY entry;
    BOOL removed;

    // These fields are for `FwpsFlowRemoveContext0`

    UINT64 flow_handle;
    UINT16 layer_id;
    UINT32 callout_id;

    // Actual context data

    struct _WFPTracer *tracer;
    UINT64 pid;
} FlowContext;

typedef struct _RegisteredCallout
{
    UINT32 fwps_callout_id;
    GUID fwpm_callout_key;
    UINT64 fwpm_filter_id;
} RegisteredCallout;

typedef struct _WFPTracer
{
    HANDLE filter_engine;
    RegisteredCallout callouts[2][2][2]; // [in/out][v4/v6][auth/transport]

    LIST_ENTRY flow_ctx_head;
    KSPIN_LOCK flow_ctx_lock;

    BOOL unloading;
    EX_SPIN_LOCK unloading_lock;
    PDEVICE_OBJECT device;

    void (*callback)(PDEVICE_OBJECT device, UINT64 pid, SIZE_T size);
} WFPTracer;

static NTSTATUS _register_callout(
    IN PDEVICE_OBJECT device,
    IN const FWPS_CALLOUT0 *callout,
    IN const GUID *fwpm_applicable_layer,
    IN const FWPM_SUBLAYER0 *sublayer,
    IN OUT WFPTracer *tracer,
    OUT RegisteredCallout *result)
{
    if (callout == NULL || sublayer == NULL || result == NULL)
    {
        LOG("At least 1 parameter is NULL");
        return STATUS_INVALID_PARAMETER;
    }

    HANDLE filter_engine = tracer->filter_engine;
    if (filter_engine == NULL)
    {
        LOG("Filter engine has not been initialized yet");
        return STATUS_UNSUCCESSFUL;
    }

    UINT32 fwps_callout_id = 0;
    NTSTATUS status = FwpsCalloutRegister0(device, callout, &fwps_callout_id);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register callout: 0x%08X", status);
        return status;
    }

    FWPM_CALLOUT0 mcallout = {0};
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

    FWPM_FILTER0 filter = {0};
    filter.displayData.name = L"Windows Listener WFP filter";
    filter.flags = FWPM_FILTER_FLAG_NONE;
    filter.layerKey = *fwpm_applicable_layer;
    filter.subLayerKey = WINDOWS_LISTENER_WFP_SUBLAYER;
    filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
    filter.action.calloutKey = callout->calloutKey;
    filter.rawContext = (UINT64)tracer;

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

static BOOL _unregister_callout(IN OUT WFPTracer *tracer, IN OUT RegisteredCallout *callout)
{
    BOOL success = TRUE;
    HANDLE filter_engine = tracer->filter_engine;
    if (filter_engine == NULL)
    {
        LOG("Cannot fully unregister callout because filter engine is uninitialized");
        success = FALSE;
    }
    else
    {
        if (callout->fwpm_filter_id != 0)
        {
            ON_DWERROR(
                FwpmFilterDeleteById0(filter_engine, callout->fwpm_filter_id),
                success = FALSE,
                "Cannot remove filter from filter engine");
            callout->fwpm_filter_id = 0;
        }

        if (!IsEqualGUID(&callout->fwpm_callout_key, &WINDOWS_LISTENER_GUID_NULL))
        {
            ON_DWERROR(
                FwpmCalloutDeleteByKey0(filter_engine, &callout->fwpm_callout_key),
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
            "Cannot unregister callout");
        callout->fwps_callout_id = 0;
    }

    return success;
}

static void NTAPI _ale_classify(
    IN const FWPS_INCOMING_VALUES0 *in_fixed_values,
    IN const FWPS_INCOMING_METADATA_VALUES0 *in_meta_values,
    IN OUT void *layer_data,
    IN const FWPS_FILTER0 *filter,
    IN UINT64 flow_context,
    IN OUT FWPS_CLASSIFY_OUT0 *classify_out)
{
    UNREFERENCED_PARAMETER(layer_data);
    classify_out->actionType = FWP_ACTION_PERMIT;

    WFPTracer *tracer = (WFPTracer *)filter->context;
    KIRQL old_irql = ExAcquireSpinLockShared(&tracer->unloading_lock);

    if (!tracer->unloading &&
        in_fixed_values != NULL &&
        in_meta_values != NULL &&
        FWPS_IS_METADATA_FIELD_PRESENT(in_meta_values, FWPS_METADATA_FIELD_PROCESS_ID) &&
        FWPS_IS_METADATA_FIELD_PRESENT(in_meta_values, FWPS_METADATA_FIELD_FLOW_HANDLE) &&
        flow_context == 0)
    {
        UINT64 pid = in_meta_values->processId;
        UINT16 layer_id = 0;
        UINT32 callout_id = 0;

        if (in_fixed_values->layerId == FWPS_LAYER_ALE_AUTH_CONNECT_V4)
        {
            layer_id = FWPS_LAYER_OUTBOUND_TRANSPORT_V4;
            callout_id = tracer->callouts[1][0][0].fwps_callout_id;
        }
        else if (in_fixed_values->layerId == FWPS_LAYER_ALE_AUTH_CONNECT_V6)
        {
            layer_id = FWPS_LAYER_OUTBOUND_TRANSPORT_V6;
            callout_id = tracer->callouts[1][1][0].fwps_callout_id;
        }
        else if (in_fixed_values->layerId == FWPS_LAYER_ALE_AUTH_RECV_ACCEPT_V4)
        {
            layer_id = FWPS_LAYER_INBOUND_TRANSPORT_V4;
            callout_id = tracer->callouts[0][0][0].fwps_callout_id;
        }
        else if (in_fixed_values->layerId == FWPS_LAYER_ALE_AUTH_RECV_ACCEPT_V6)
        {
            layer_id = FWPS_LAYER_INBOUND_TRANSPORT_V6;
            callout_id = tracer->callouts[0][1][0].fwps_callout_id;
        }
        else
        {
            LOG("Warning: _ale_classify called on unsupported layer ID %u", in_fixed_values->layerId);
            goto cleanup;
        }

        FlowContext *ctx = (FlowContext *)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(FlowContext), POOL_TAG);
        if (ctx == NULL)
        {
            LOG("Warning: Insufficient memory to allocate flow context");
        }
        else
        {
            ctx->removed = FALSE;
            ctx->flow_handle = in_meta_values->flowHandle;
            ctx->layer_id = layer_id;
            ctx->callout_id = callout_id;
            ctx->tracer = tracer;
            ctx->pid = pid;

            // In order to use `FwpsFlowAssociateContext0`, we need to define a `flow_delete` function
            // below (even a dummy one works), or else this call will return `STATUS_INVALID_PARAMETER`.
            NTSTATUS status = FwpsFlowAssociateContext0(ctx->flow_handle, ctx->layer_id, ctx->callout_id, (UINT64)ctx);
            if (NT_SUCCESS(status))
            {
                KIRQL irql = KeAcquireSpinLockRaiseToDpc(&tracer->flow_ctx_lock);
                InsertTailList(&tracer->flow_ctx_head, &ctx->entry);
                KeReleaseSpinLock(&tracer->flow_ctx_lock, irql);
            }
            else
            {
                ExFreePool(ctx);
                LOG("Warning: Failed to associate flow context to layer: 0x%08X", status);
            }
        }
    }

cleanup:
    // This release must stay at the end of the function. This is to guarantee that the last function
    // execution with `UNLOADING = FALSE` is fully done before `driver_unload` proceeds.
    ExReleaseSpinLockShared(&tracer->unloading_lock, old_irql);
}

static void NTAPI _transport_classify(
    IN const FWPS_INCOMING_VALUES0 *in_fixed_values,
    IN const FWPS_INCOMING_METADATA_VALUES0 *in_meta_values,
    IN OUT void *layer_data,
    IN const FWPS_FILTER0 *filter,
    IN UINT64 flow_context,
    IN OUT FWPS_CLASSIFY_OUT0 *classify_out)
{
    UNREFERENCED_PARAMETER(in_fixed_values);
    UNREFERENCED_PARAMETER(in_meta_values);
    classify_out->actionType = FWP_ACTION_PERMIT;

    WFPTracer *tracer = (WFPTracer *)filter->context;
    KIRQL old_irql = ExAcquireSpinLockShared(&tracer->unloading_lock);

    if (!tracer->unloading && flow_context != 0)
    {
        FlowContext *ctx = (FlowContext *)flow_context;
        UINT64 pid = ctx->pid;
        if (pid != 0 && layer_data != NULL)
        {
            const NET_BUFFER_LIST *data = (const NET_BUFFER_LIST *)layer_data;

            SIZE_T total = 0;
            for (const NET_BUFFER_LIST *nbl = data; nbl != NULL; nbl = NET_BUFFER_LIST_NEXT_NBL(nbl))
            {
                for (const NET_BUFFER *nb = NET_BUFFER_LIST_FIRST_NB(nbl); nb != NULL; nb = NET_BUFFER_NEXT_NB(nb))
                {
                    total += NET_BUFFER_DATA_LENGTH(nb);
                }
            }

            tracer->callback(tracer->device, pid, total);
        }
    }

    // This release must stay at the end of the function. See the note in `_ale_classify`.
    ExReleaseSpinLockShared(&tracer->unloading_lock, old_irql);
}

static NTSTATUS NTAPI notify(
    IN FWPS_CALLOUT_NOTIFY_TYPE notify_type,
    IN const GUID *filter_key,
    IN const FWPS_FILTER0 *filter)
{
    UNREFERENCED_PARAMETER(notify_type);
    UNREFERENCED_PARAMETER(filter_key);
    UNREFERENCED_PARAMETER(filter);
    return STATUS_SUCCESS;
}

static void NTAPI _transport_flow_delete(
    IN UINT16 layer_id,
    IN UINT32 callout_id,
    IN UINT64 flow_context)
{
    UNREFERENCED_PARAMETER(layer_id);
    UNREFERENCED_PARAMETER(callout_id);
    LOG("Deleting flow %u %u %u", layer_id, callout_id, flow_context);

    if (flow_context != 0)
    {
        FlowContext *ctx = (FlowContext *)flow_context;

        KIRQL old_irql = KeAcquireSpinLockRaiseToDpc(&ctx->tracer->flow_ctx_lock);
        if (!ctx->removed)
        {
            ctx->removed = TRUE;
            RemoveEntryList(&ctx->entry);
        }

        KeReleaseSpinLock(&ctx->tracer->flow_ctx_lock, old_irql);
        ExFreePool(ctx);
    }
}

const FWPS_CALLOUT0 INCOMING_ALE_AUTH_V4_CALLOUT = {
    {0xda4d8b9a, 0x7d13, 0x4c89, {0x9b, 0x3c, 0xb4, 0x44, 0x73, 0x18, 0x2f, 0x93}},
    0,
    _ale_classify,
    notify,
    NULL};

const FWPS_CALLOUT0 INCOMING_ALE_AUTH_V6_CALLOUT = {
    {0xda4d8b9a, 0x7d13, 0x4c89, {0x9b, 0x3c, 0xb4, 0x44, 0x73, 0x18, 0x2f, 0x94}},
    0,
    _ale_classify,
    notify,
    NULL};

const FWPS_CALLOUT0 OUTGOING_ALE_AUTH_V4_CALLOUT = {
    {0xda4d8b9a, 0x7d13, 0x4c89, {0x9b, 0x3c, 0xb4, 0x44, 0x73, 0x18, 0x30, 0x93}},
    0,
    _ale_classify,
    notify,
    NULL};

const FWPS_CALLOUT0 OUTGOING_ALE_AUTH_V6_CALLOUT = {
    {0xda4d8b9a, 0x7d13, 0x4c89, {0x9b, 0x3c, 0xb4, 0x44, 0x73, 0x18, 0x30, 0x94}},
    0,
    _ale_classify,
    notify,
    NULL};

const FWPS_CALLOUT0 INCOMING_ALE_TRANSPORT_V4_CALLOUT = {
    {0x37d2ded2, 0xce93, 0x4e2a, {0xb6, 0x77, 0xd8, 0x0c, 0x73, 0x4f, 0x70, 0x95}},
    0,
    _transport_classify,
    notify,
    _transport_flow_delete};

const FWPS_CALLOUT0 INCOMING_ALE_TRANSPORT_V6_CALLOUT = {
    {0x37d2ded2, 0xce93, 0x4e2a, {0xb6, 0x77, 0xd8, 0x0c, 0x73, 0x4f, 0x70, 0x96}},
    0,
    _transport_classify,
    notify,
    _transport_flow_delete};

const FWPS_CALLOUT0 OUTGOING_ALE_TRANSPORT_V4_CALLOUT = {
    {0x37d2ded2, 0xce93, 0x4e2a, {0xb6, 0x77, 0xd8, 0x0c, 0x73, 0x4f, 0x71, 0x95}},
    0,
    _transport_classify,
    notify,
    _transport_flow_delete};

const FWPS_CALLOUT0 OUTGOING_ALE_TRANSPORT_V6_CALLOUT = {
    {0x37d2ded2, 0xce93, 0x4e2a, {0xb6, 0x77, 0xd8, 0x0c, 0x73, 0x4f, 0x71, 0x96}},
    0,
    _transport_classify,
    notify,
    _transport_flow_delete};

void free_wfp_tracer(WFPTracerHandle tracer)
{
    LOG("Freeing WFP tracer");

    KIRQL old_irql = ExAcquireSpinLockExclusive(&tracer->unloading_lock);
    tracer->unloading = TRUE;
    ExReleaseSpinLockExclusive(&tracer->unloading_lock, old_irql);

    // At this point, it is guaranteed that no new flow contexts will be created and read.
    // In other words, both `_ale_classify` and `_transport_classify` become no-op functions.

    LOG("All ongoing WFP classify calls are finished");
    old_irql = KeAcquireSpinLockRaiseToDpc(&tracer->flow_ctx_lock);

    UINT32 counter = 0;
    while (!IsListEmpty(&tracer->flow_ctx_head))
    {
        PLIST_ENTRY node = tracer->flow_ctx_head.Flink;
        FlowContext *ctx = CONTAINING_RECORD(node, FlowContext, entry);
        LOG("Removing flow context %u", ++counter);

        if (!ctx->removed)
        {
            ctx->removed = TRUE;
            RemoveEntryList(&ctx->entry);
        }

        // Release the spin lock so that `_transport_flow_delete` can acquire it
        KeReleaseSpinLock(&tracer->flow_ctx_lock, old_irql);

        // Trigger flow_delete, but DO NOT free memory (i.e. `ExFreePool`) here
        FwpsFlowRemoveContext0(ctx->flow_handle, ctx->layer_id, ctx->callout_id);

        old_irql = KeAcquireSpinLockRaiseToDpc(&tracer->flow_ctx_lock);
    }

    // Release to restore IRQL, after this point no one will use the lock anyway
    KeReleaseSpinLock(&tracer->flow_ctx_lock, old_irql);

    if (tracer->filter_engine != NULL)
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                for (int k = 0; k < 2; k++)
                {
                    if (!_unregister_callout(tracer, &tracer->callouts[i][j][k]))
                    {
                        LOG("Failed to unregister callout [%d][%d][%d]", i, j, k);
                    }
                }
            }
        }

        INSPECT_DWERROR(
            FwpmSubLayerDeleteByKey0(tracer->filter_engine, &WINDOWS_LISTENER_WFP_SUBLAYER),
            "Cannot remove sublayer WINDOWS_LISTENER_WFP_SUBLAYER");
        INSPECT_DWERROR(
            FwpmEngineClose0(tracer->filter_engine),
            "Cannot close WFP engine");
        tracer->filter_engine = NULL;
    }

    ExFreePool(tracer);
}

static void _dummy_callback(PDEVICE_OBJECT device, UINT64 pid, SIZE_T size)
{
    UNREFERENCED_PARAMETER(device);
    UNREFERENCED_PARAMETER(pid);
    UNREFERENCED_PARAMETER(size);
}

WFPTracerHandle new_wfp_tracer(PDEVICE_OBJECT device, void (*callback)(PDEVICE_OBJECT device, UINT64 pid, SIZE_T size))
{
    LOG("Initializing new WFP tracer");
    WFPTracer *tracer = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(WFPTracer), POOL_TAG);
    if (tracer == NULL)
    {
        LOG("Cannot allocate for WFPTracer");
        return NULL;
    }

    RtlZeroMemory(tracer, sizeof(WFPTracer));
    InitializeListHead(&tracer->flow_ctx_head);
    KeInitializeSpinLock(&tracer->flow_ctx_lock);
    tracer->device = device;
    tracer->callback = (callback == NULL ? _dummy_callback : callback);

    DWORD ustatus = FwpmEngineOpen0(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &tracer->filter_engine);
    if (ustatus != ERROR_SUCCESS)
    {
        LOG("Failed to open filter engine: 0x%08X", ustatus);
        free_wfp_tracer(tracer);
        return NULL;
    }

    FWPM_SUBLAYER0 sublayer = {0};
    sublayer.subLayerKey = WINDOWS_LISTENER_WFP_SUBLAYER;
    sublayer.displayData.name = L"Windows Listener WFP Sublayer";
    sublayer.displayData.description = L"Sublayer for Windows Listener WFP Callout driver";
    sublayer.weight = 0x100;

    ustatus = FwpmSubLayerAdd0(tracer->filter_engine, &sublayer, NULL);
    if (ustatus != ERROR_SUCCESS && ustatus != ERROR_ALREADY_EXISTS)
    {
        LOG("Failed to add sublayer: 0x%08X", ustatus);
        free_wfp_tracer(tracer);
        return NULL;
    }

    NTSTATUS status = _register_callout(
        device,
        &INCOMING_ALE_TRANSPORT_V4_CALLOUT,
        &FWPM_LAYER_INBOUND_TRANSPORT_V4,
        &sublayer,
        tracer,
        &tracer->callouts[0][0][0]);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register INCOMING_ALE_TRANSPORT_V4_CALLOUT: 0x%08X", status);
        free_wfp_tracer(tracer);
        return NULL;
    }

    status = _register_callout(
        device,
        &INCOMING_ALE_TRANSPORT_V6_CALLOUT,
        &FWPM_LAYER_INBOUND_TRANSPORT_V6,
        &sublayer,
        tracer,
        &tracer->callouts[0][1][0]);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register INCOMING_ALE_TRANSPORT_V6_CALLOUT: 0x%08X", status);
        free_wfp_tracer(tracer);
        return NULL;
    }

    status = _register_callout(
        device,
        &OUTGOING_ALE_TRANSPORT_V4_CALLOUT,
        &FWPM_LAYER_OUTBOUND_TRANSPORT_V4,
        &sublayer,
        tracer,
        &tracer->callouts[1][0][0]);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register OUTGOING_ALE_TRANSPORT_V4_CALLOUT: 0x%08X", status);
        free_wfp_tracer(tracer);
        return NULL;
    }

    status = _register_callout(
        device,
        &OUTGOING_ALE_TRANSPORT_V6_CALLOUT,
        &FWPM_LAYER_OUTBOUND_TRANSPORT_V6,
        &sublayer,
        tracer,
        &tracer->callouts[1][1][0]);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register OUTGOING_ALE_TRANSPORT_V6_CALLOUT: 0x%08X", status);
        free_wfp_tracer(tracer);
        return NULL;
    }

    status = _register_callout(
        device,
        &INCOMING_ALE_AUTH_V4_CALLOUT,
        &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
        &sublayer,
        tracer,
        &tracer->callouts[0][0][1]);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register INCOMING_ALE_AUTH_V4_CALLOUT: 0x%08X", status);
        free_wfp_tracer(tracer);
        return NULL;
    }

    status = _register_callout(
        device,
        &INCOMING_ALE_AUTH_V6_CALLOUT,
        &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
        &sublayer,
        tracer,
        &tracer->callouts[0][1][1]);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register INCOMING_ALE_AUTH_V6_CALLOUT: 0x%08X", status);
        free_wfp_tracer(tracer);
        return NULL;
    }

    status = _register_callout(
        device,
        &OUTGOING_ALE_AUTH_V4_CALLOUT,
        &FWPM_LAYER_ALE_AUTH_CONNECT_V4,
        &sublayer,
        tracer,
        &tracer->callouts[1][0][1]);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register OUTGOING_ALE_AUTH_V4_CALLOUT: 0x%08X", status);
        free_wfp_tracer(tracer);
        return NULL;
    }

    status = _register_callout(
        device,
        &OUTGOING_ALE_AUTH_V6_CALLOUT,
        &FWPM_LAYER_ALE_AUTH_CONNECT_V6,
        &sublayer,
        tracer,
        &tracer->callouts[1][1][1]);
    if (!NT_SUCCESS(status))
    {
        LOG("Failed to register OUTGOING_ALE_AUTH_V6_CALLOUT: 0x%08X", status);
        free_wfp_tracer(tracer);
        return NULL;
    }

    return tracer;
}
