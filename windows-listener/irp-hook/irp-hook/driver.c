#include <wdm.h>

PDRIVER_DISPATCH original_irp_handlers[IRP_MJ_MAXIMUM_FUNCTION + 1] = {NULL};
PDRIVER_OBJECT target_driver = NULL;
PFILE_OBJECT target_file = NULL;

static void driver_unload(PDRIVER_OBJECT driver)
{
    UNREFERENCED_PARAMETER(driver);

    if (target_driver != NULL)
    {
        for (UCHAR i = 0; i < IRP_MJ_MAXIMUM_FUNCTION + 1; i++)
        {
            target_driver->MajorFunction[i] = original_irp_handlers[i];
        }
        target_driver = NULL;
    }

    if (target_file != NULL)
    {
        ObDereferenceObject(target_file);
        target_file = NULL;
    }
}

NTSTATUS hooked_irp_handler(PDEVICE_OBJECT device, PIRP irp)
{
    DbgPrint("hooked_irp_handler called\n");

    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    UCHAR major = stack->MajorFunction;
    DbgPrint("MajorFunction: %d\n", major);

    return original_irp_handlers[major](device, irp);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    UNREFERENCED_PARAMETER(registry);
    driver->DriverUnload = driver_unload;

    UNICODE_STRING target_device_name = RTL_CONSTANT_STRING(L"\\Device\\WinLisDev");
    PDEVICE_OBJECT target_device = NULL;
    NTSTATUS status = IoGetDeviceObjectPointer(
        &target_device_name,
        FILE_READ_DATA,
        &target_file,
        &target_device);

    if (!NT_SUCCESS(status))
    {
        DbgPrint("IoGetDeviceObjectPointer failed: 0x%X\n", status);
        driver_unload(driver);
        return status;
    }

    target_driver = target_device->DriverObject;
    for (UCHAR i = 0; i < IRP_MJ_MAXIMUM_FUNCTION + 1; i++)
    {
        original_irp_handlers[i] = target_driver->MajorFunction[i];
        if (target_driver->MajorFunction[i] != NULL)
        {
            target_driver->MajorFunction[i] = hooked_irp_handler;
        }
    }

    return STATUS_SUCCESS;
}
