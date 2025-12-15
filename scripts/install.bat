@echo off
:: Driver installation script for Windows VM

for %%f in ("%~dp0") do set root=%%~ff

set name=WindowsListenerService
if not "%~1"=="" set name=%~1

:: We add these keys for compatibility with older versions.
reg add "HKLM\SYSTEM\CurrentControlSet\Services\%name%" /v SupportedFeatures /t REG_DWORD /d 3 /f
reg add "HKLM\SYSTEM\CurrentControlSet\Services\%name%\Instances" /v DefaultInstance /t REG_SZ /d "Windows Listener Instance" /f
reg add "HKLM\SYSTEM\CurrentControlSet\Services\%name%\Instances\Windows Listener Instance" /v Altitude /t REG_SZ /d "360000" /f
reg add "HKLM\SYSTEM\CurrentControlSet\Services\%name%\Instances\Windows Listener Instance" /v Flags /t REG_DWORD /d 0 /f

:: The following values should be under the Parameters subkey starting with Windows 11 version 24H2.
:: See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/creating-an-inf-file-for-a-minifilter-driver
reg add "HKLM\SYSTEM\CurrentControlSet\Services\%name%\Parameters\Parameters" /v SupportedFeatures /t REG_DWORD /d 3 /f
reg add "HKLM\SYSTEM\CurrentControlSet\Services\%name%\Parameters\Instances" /v DefaultInstance /t REG_SZ /d "Windows Listener Instance" /f
reg add "HKLM\SYSTEM\CurrentControlSet\Services\%name%\Parameters\Instances\Windows Listener Instance" /v Altitude /t REG_SZ /d "360000" /f
reg add "HKLM\SYSTEM\CurrentControlSet\Services\%name%\Parameters\Instances\Windows Listener Instance" /v Flags /t REG_DWORD /d 0 /f

@echo on
sc create "%name%" binPath= "%root%windows_listener_driver.sys" type= filesys start= demand depend= FltMgr
