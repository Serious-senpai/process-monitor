@echo off
:: Setup script for Windows VM

for %%f in ("%~dp0") do set root=%%~ff

curl -L -o "%root%\DebugView.zip" https://download.sysinternals.com/files/DebugView.zip
powershell Expand-Archive -Force "%root%\DebugView.zip" "%root%\DebugView"
del "%root%\DebugView.zip"

curl -L -o "%root%\WFPExp.exe" https://github.com/zodiacon/WFPExplorer/releases/download/v0.5.4/WFPExp.exe
curl -L -o "%root%\PoolMonX.exe" https://github.com/zodiacon/PoolMonXv3/releases/download/v3.0.1/PoolMonX.exe

echo @echo off>%root%\run.bat
echo cd /d "%root%">>%root%\run.bat
echo cmd>>%root%\run.bat
