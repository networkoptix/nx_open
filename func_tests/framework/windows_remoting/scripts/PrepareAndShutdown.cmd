@rem Disable WinRM rules, enable them in SetupComplete.cmd via quickconfig.
rem powershell -Command "Disable-NetFirewallRule -DisplayGroup 'Windows Remote Management'"

@echo Remove Appx packages that can be removed.
@echo They can cause SysPrep to fail.
@echo See: https://social.technet.microsoft.com/Forums/windows/en-US/c18af9ed-bd5b-4eac-bf03-bec89aca1ae3
@echo Expect many error messages about packages that cannot be removed.
rem powershell -Command "Get-AppxPackage | Remove-AppxPackage"

@echo Defragment, consolidate free space on disk.
@echo Zero-fill free space to make virtual disk size less.
rem defrag C: /X /H /U /V
rem sdelete /accepteula -z C:

@echo Copy script to run on first logon.
copy /V /Y %~dp0\FirstLogon.cmd C:\Users\Public\Desktop\FirstLogon.cmd
type C:\Users\Public\Desktop\FirstLogon.cmd

@echo SysPrep will shutdown machine.
C:\Windows\System32\SysPrep\SysPrep.exe /generalize /oobe /shutdown /unattend:%~dp0\Autounattend.xml
