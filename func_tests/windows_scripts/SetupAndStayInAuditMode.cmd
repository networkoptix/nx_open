powershell -NonInteractive -ExecutionPolicy Unrestricted -File %~dp0\SetupMiscellaneous.ps1
powershell -NonInteractive -ExecutionPolicy Unrestricted -File %~dp0\MakeAllConnectionsPrivate.ps1
call %~dp0\SetupWinRM.cmd
@pause
