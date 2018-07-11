@rem All settings here should be done just before SysPrep.

@rem Disable WinRM rules, enable them in SetupComplete.cmd via quickconfig.
powershell -NonInteractive -ExecutionPolicy Unrestricted -Command "Disable-NetFirewallRule -DisplayGroup 'Windows Remote Management'"

@echo Defragment, consolidate free space on disk.
@echo Zero-fill free space to make virtual disk size less.
defrag C: /X /H /U /V
sdelete /accepteula -z C:

@echo Copy script to run on first logon.
copy /V /Y %~dp0\FirstLogon.ps1 C:\Users\Public\Desktop\FirstLogon.ps1

@echo SysPrep will shutdown machine. GUI instance, which was shown at startup in Audit mode is killed.
TaskKill /IM SysPrep.exe /F
C:\Windows\System32\SysPrep\SysPrep.exe /generalize /oobe /shutdown /unattend:%~dp0\DeployAutounattend.xml
