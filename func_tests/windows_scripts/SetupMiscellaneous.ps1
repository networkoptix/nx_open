Set-ExecutionPolicy Unrestricted -Force
Enable-NetFirewallRule -DisplayGroup 'File and Printer Sharing'  # Pings are part of this.
Enable-NetFirewallRule -DisplayGroup 'Network Discovery'
Enable-NetFirewallRule -DisplayGroup 'Remote Desktop'
Set-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System' 'ConsentPromptBehaviorAdmin' 0
Set-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System' 'ConsentPromptBehaviorUser' 0
New-Item 'HKLM:\SYSTEM\CurrentControlSet\Control\Network\NewNetworkWindowOff' -Force
Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Power' 'HibernateFileSizePercent' 0
Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Power' 'HibernateEnabled' 0

Set-Service SSDPSRV -StartupType Disabled
Set-Service fdPHost -StartupType Disabled
Set-Service FDResPub -StartupType Disabled
Set-Service BITS -StartupType Disabled  # Background downloads.
Set-Service DusmSvc -StartupType Disabled  # Data usage metrics.
Set-Service HomeGroupProvider -StartupType Disabled
Set-Service lfsvc -StartupType Disabled  # Geolocation.
Set-Service wlidsvc -StartupType Disabled  # Sign-in with MS Live account.
