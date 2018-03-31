@rem Allow all PowerShell scripts.
powershell -ExecutionPolicy Unrestricted -Command "Set-ExecutionPolicy Unrestricted -Force"

powershell -Command "Set-ItemProperty -Path HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System\ -Name ConsentPromptBehaviorAdmin -Value 0"
powershell -Command "Set-ItemProperty -Path HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System\ -Name ConsentPromptBehaviorUser -Value 0"

@rem Pings are part of File and Printer Sharing.
powershell -Command "Enable-NetFirewallRule -DisplayGroup 'File and Printer Sharing'"
powershell -Command "Enable-NetFirewallRule -DisplayGroup 'Network Discovery'"

@rem WinRM warns and doesn't work on public networks.
@rem Client setup is not required but is here for debugging purposes.
powershell -Command "Set-NetConnectionProfile -NetworkCategory Private"
call winrm quickconfig -quiet
call winrm set winrm/config @{MaxEnvelopeSizekb="65536"}
call winrm set winrm/config/Service @{AllowUnencrypted="true"}
call winrm set winrm/config/Service/Auth @{Basic="true"}
call winrm set winrm/config/Client @{AllowUnencrypted="true"}
call winrm set winrm/config/Client @{TrustedHosts="*"}
call winrm set winrm/config/Client/Auth @{Basic="true"}
call sc config WinRM start= auto

@echo Completed.
@pause
