powershell -NonInteractive -ExecutionPolicy Unrestricted -File %~dp0\MakeAllConnectionsPrivate.ps1
powershell -NonInteractive -ExecutionPolicy Unrestricted -Command "Enable-NetFirewallRule -DisplayGroup 'Windows Remote Management'"
@pause
