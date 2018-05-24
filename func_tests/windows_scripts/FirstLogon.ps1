Enable-NetFirewallRule -DisplayGroup 'Windows Remote Management'
do {
    Set-NetConnectionProfile -NetworkCategory Private -ErrorAction SilentlyContinue
    $connectionProfiles = Get-NetConnectionProfile
    $connectionProfiles | Format-Table Name, InterfaceAlias,InterfaceIndex,IPv4Connectivity,NetworkCategory
} while ($connectionProfiles |? { $_.NetworkCategory -ne 'Private' })

New-Item 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU' -Force
Set-ItemProperty 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU' 'NoAutoUpdate' 1
Stop-Service wuauserv
Set-Service wuauserv -StartupType Manual

Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'Hidden' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'HideFileExt' 0
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'ShowSuperHidden' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'Start_ShowRun' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'StartMenuAdminTools' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'TaskbarGlomLevel' 2
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'TaskbarSmallIcons' 1
