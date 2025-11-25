@echo off

for %%f in ("%~dp0") do set root=%%~ff

@echo on
sc create WindowsListenerService binPath= "%root%\windows_listener_driver.sys" type=kernel depend= FltMgr
reg add "HKLM\SYSTEM\CurrentControlSet\Services\WindowsListenerService\Parameters\Instances" /v DefaultInstance /t REG_SZ /d "Windows Listener Instance"
reg add "HKLM\SYSTEM\CurrentControlSet\Services\WindowsListenerService\Parameters\Instances\Windows Listener Instance" /v Altitude /t REG_SZ /d "370030"
reg add "HKLM\SYSTEM\CurrentControlSet\Services\WindowsListenerService\Parameters\Instances\Windows Listener Instance" /v Flags /t REG_DWORD /d 0
