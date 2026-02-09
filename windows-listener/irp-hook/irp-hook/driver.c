#include <wdm.h>

PDRIVER_OBJECT target_driver = NULL;
PFILE_OBJECT target_file = NULL;

static void driver_unload(PDRIVER_OBJECT driver)
{
    UNREFERENCED_PARAMETER(driver);

    if (target_file != NULL)
    {
        ObDereferenceObject(target_file);
        target_file = NULL;
    }
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
        &target_device
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("IoGetDeviceObjectPointer failed: 0x%X\n", status);
        driver_unload(driver);
        return status;
    }

    target_driver = target_device->DriverObject;
    return STATUS_SUCCESS;
}
