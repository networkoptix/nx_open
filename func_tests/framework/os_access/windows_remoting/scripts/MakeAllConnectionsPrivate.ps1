do
{
    Set-NetConnectionProfile -NetworkCategory Private -ErrorAction SilentlyContinue
    $connectionProfiles = Get-NetConnectionProfile
    $connectionProfiles | Format-Table Name, InterfaceAlias, InterfaceIndex, IPv4Connectivity, NetworkCategory
}
while ($connectionProfiles |? { $_.NetworkCategory -ne 'Private' })