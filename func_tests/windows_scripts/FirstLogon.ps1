Enable-NetFirewallRule -DisplayGroup 'Windows Remote Management'
do {
    Set-NetConnectionProfile -NetworkCategory Private -ErrorAction SilentlyContinue
    $connectionProfiles = Get-NetConnectionProfile
    $connectionProfiles | Format-Table Name, InterfaceAlias,InterfaceIndex,IPv4Connectivity,NetworkCategory
} while ($connectionProfiles |? { $_.NetworkCategory -ne 'Private' })


Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'Hidden' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'HideFileExt' 0
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'ShowSuperHidden' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'Start_ShowRun' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'StartMenuAdminTools' 1
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'TaskbarGlomLevel' 2
Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'TaskbarSmallIcons' 1
Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\LanmanWorkstation\Parameters' 'AllowInsecureGuestAuth' 1
