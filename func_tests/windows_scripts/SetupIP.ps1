Get-NetAdapter
$InterfaceIndex = Read-Host -Prompt "InterfaceIndex"
$LastOctet = Read-Host -Prompt "LastOctet"
New-NetIPAddress -InterfaceIndex $InterfaceIndex -IPAddress 10.254.252.$LastOctet -PrefixLength 24