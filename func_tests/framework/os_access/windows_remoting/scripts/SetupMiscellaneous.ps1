Set-ExecutionPolicy Unrestricted -Force
Enable-NetFirewallRule -DisplayGroup 'File and Printer Sharing'  # Pings are part of this.
Enable-NetFirewallRule -DisplayGroup 'Network Discovery'
Enable-NetFirewallRule -DisplayGroup 'Remote Desktop'
Set-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System' 'ConsentPromptBehaviorAdmin' 0
Set-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System' 'ConsentPromptBehaviorUser' 0
New-Item 'HKLM:\SYSTEM\CurrentControlSet\Control\Network\NewNetworkWindowOff' -Force
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'Hidden' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'HideFileExt' 0
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'ShowSuperHidden' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'Start_ShowRun' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'StartMenuAdminTools' 1
Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Power' 'HibernateFileSizePercent' 0
Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Power' 'HibernateEnabled' 0
