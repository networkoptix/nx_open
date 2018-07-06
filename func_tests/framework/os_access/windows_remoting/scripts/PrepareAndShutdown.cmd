@rem All settings here should be done just before SysPrep.

@rem Disable WinRM rules, enable them in SetupComplete.cmd via quickconfig.
powershell -NonInteractive -ExecutionPolicy Unrestricted -Command "Disable-NetFirewallRule -DisplayGroup 'Windows Remote Management'"

@echo Remove Appx packages that can be removed.
@echo They can cause SysPrep to fail.
@echo See: https://social.technet.microsoft.com/Forums/windows/en-US/c18af9ed-bd5b-4eac-bf03-bec89aca1ae3
@echo Expect many error messages about packages that cannot be removed.
powershell -NonInteractive -ExecutionPolicy Unrestricted -Command "Get-AppxPackage | Remove-AppxPackage" -ErrorAction SilentlyContinue

@echo Defragment, consolidate free space on disk.
@echo Zero-fill free space to make virtual disk size less.
defrag C: /X /H /U /V
sdelete /accepteula -z C:

@echo Copy script to run on first logon.
copy /V /Y %~dp0\FirstLogon.cmd C:\Users\Public\Desktop\FirstLogon.cmd
copy /V /Y %~dp0\MakeAllConnectionsPrivate.ps1 C:\Users\Public\Desktop\MakeAllConnectionsPrivate.ps1

@echo SysPrep will shutdown machine.
C:\Windows\System32\SysPrep\SysPrep.exe /generalize /oobe /shutdown /unattend:%~dp0\Autounattend.xml
