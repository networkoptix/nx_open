import logging

from netaddr import EUI, mac_eui48
from pylru import lrudecorator

from framework.networking.interface import Networking
from framework.os_access.windows_remoting.cmd.powershell import PowershellError

_logger = logging.getLogger(__name__)


class PingError(Exception):
    def __init__(self, ip, message):
        super(PingError, self).__init__(message)
        self.ip = ip


class WindowsNetworking(Networking):
    _firewall_rule_name = 'NX-TestStandNetwork'
    _firewall_rule_display_name = 'NX Test Stand Network'

    def __init__(self, winrm_access, macs):
        super(WindowsNetworking, self).__init__()
        self._names = {mac: 'Plugable {}'.format(slot) for slot, mac in macs.items()}
        self._winrm_access = winrm_access
        self.__repr__ = lambda: '<WindowsNetworking on {}>'.format(winrm_access)

    def rename_interfaces(self, mac_to_new_name):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                $adapters = @( Get-NetAdapter )
                foreach ( $macAndNewName in $macToNewName ) {
                    $mac = $macAndNewName[0]
                    $newName = $macAndNewName[1]
                    $renamed = $false
                    foreach ( $adapter in $adapters ) {
                        if ( $adapter.MacAddress -eq $mac ) {
                            Rename-NetAdapter -Name:$adapter.Name -NewName:$newName
                            $renamed = $true
                            [Console]::Error.WriteLine("Renamed adapter with MAC $mac to $newName")
                        }
                    }
                    if ( -not $renamed ) {
                        [Console]::Error.WriteLine("No adapter with MAC $mac")
                    }
                }
                [Console]::Error.WriteLine("Name and InterfaceAlias are synonyms")
                [Console]::Error.WriteLine("InterfaceIndex may chane after reboot, don't rely on it!")
                ''',
            {
                'macToNewName': [
                    (str(EUI(mac, dialect=mac_eui48)), new_name)
                    for mac, new_name in mac_to_new_name.items()],
                })

    @property
    @lrudecorator(1)
    def interfaces(self):
        self.rename_interfaces(self._names)
        return self._names

    def firewall_rule_exists(self):
        rules = self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                Get-NetFirewallRule -Name:$Name -ErrorAction:SilentlyContinue |
                    select Name,DisplayName,Direction,RemoteAddress,Action
                ''',
            {'name': self._firewall_rule_name})
        return bool(rules)

    def create_firewall_rule(self):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                if ( -not ( Get-NetFirewallRule -Name:$Name -ErrorAction:SilentlyContinue ) ) {
                    $newRule = New-NetFirewallRule `
                        -Name:$Name `
                        -DisplayName:$DisplayName `
                        -Direction:Outbound `
                        -RemoteAddress:@('10.0.0.0/8', '192.168.0.0/16') `
                        -Action:Allow
                }
                ''',
            {'Name': self._firewall_rule_name, 'DisplayName': self._firewall_rule_display_name})

    def remove_firewall_rule(self):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                Remove-NetFirewallRule `
                    -Name:$Name `
                    -ErrorAction:SilentlyContinue `
                    -Confirm:$false
                ''',
            {'Name': self._firewall_rule_name})

    def disable_internet(self):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''Set-NetFirewallProfile -DefaultOutboundAction:Block''',
            {})

    def enable_internet(self):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''Set-NetFirewallProfile -DefaultOutboundAction:Allow''',
            {})

    def internet_is_enabled(self):
        all_profiles = self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''Get-NetFirewallProfile | select DefaultOutboundAction | ConvertTo-Json''')
        return not all(profile['DefaultOutboundAction'] == 'Block' for profile in all_profiles)

    def setup_ip(self, mac, ip, prefix_length):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                $newIpAddressObject = New-NetIPAddress `
                    -InterfaceAlias:$interfaceAlias `
                    -IPAddress:$ipAddress `
                    -PrefixLength:$prefixLength
                ''',
            {'interfaceAlias': self.interfaces[mac], 'ipAddress': str(ip), 'prefixLength': prefix_length})

    def list_ips(self):
        result = self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                Get-NetIPAddress -PolicyStore:PersistentStore -AddressFamily:IPv4 -ErrorAction:SilentlyContinue |
                    select InterfaceAlias,IPAddress
                    ''',
            {})
        return result

    def remove_ips(self):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            ''' 
                # Get addresses from PersistentStore and delete them and their ActiveStore counterparts.
                $ipAddresses = (Get-NetIPAddress -PolicyStore:PersistentStore)
                foreach ( $ipAddress in $ipAddresses ) {
                    Remove-NetIPAddress `
                        -InterfaceAlias:$ipAddress.InterfaceAlias `
                        -IPAddress:$ipAddress.IPAddress `
                        -Confirm:$false
                }
                ''',
            {})

    def route(self, destination_ip_net, gateway_bound_mac, gateway_ip):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                $newRoute = New-NetRoute `
                    -DestinationPrefix:$destinationPrefix `
                    -InterfaceAlias:$interfaceAlias `
                    -NextHop:$nextHop
                ''',
            {
                'destinationPrefix': destination_ip_net,
                'nextHop': gateway_ip,
                'interfaceAlias': self.interfaces[gateway_bound_mac],
                })

    def list_routes(self):
        result = self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                Get-NetRoute -PolicyStore:PersistentStore -AddressFamily:IPv4 -ErrorAction:SilentlyContinue |
                    select DestinationPrefix,InterfaceAlias,NextHop
                    ''',
            {})
        return result

    def remove_routes(self):
        self._winrm_access.run_powershell_script(
            # language=PowerShell
            '''
                $persistentRules = (Get-NetRoute -PolicyStore:PersistentStore -ErrorAction:SilentlyContinue)
                if ( $persistentRules ) {
                    Remove-NetRoute `
                        -DestinationPrefix:$persistentRules.DestinationPrefix `
                        -InterfaceAlias:$persistentRules.InterfaceAlias `
                        -NextHop:$persistentRules.NextHop `
                        -Confirm:$false
                }
                ''',
            {})

    def ping(self, ip, count=1, timeout_sec=5):
        try:
            _ = self._winrm_access.run_powershell_script(
                # language=PowerShell
                '''
                    $timer = [Diagnostics.Stopwatch]::StartNew()
                    $countLeft = $count
                    while ($true) {
                        try {
                            Test-Connection $computerName -Count 1 -Delay 1
                        } catch [System.Net.NetworkInformation.PingException] {
                            $countLeft--
                            if ($countLeft -le 0 -or $timer.Elapsed.TotalSeconds -ge $timeoutSeconds) {
                                throw
                            }
                            continue
                        }
                        break
                    }
                    ''',
                {'count': count, 'computerName': str(ip), 'timeoutSeconds': timeout_sec})
        except PowershellError as e:
            if e.type_name == 'System.Net.NetworkInformation.PingException':
                raise PingError(ip, e.message)
            raise

    def can_reach(self, ip, timeout_sec=4):
        try:
            self.ping(ip, count=100, timeout_sec=timeout_sec)
        except PingError:
            return False
        else:
            return True

    def reset(self):
        self.remove_routes()
        self.remove_ips()
        self.create_firewall_rule()
        self.disable_internet()

    def setup_nat(self, outer_mac):
        raise NotImplementedError("Windows 10 cannot be set up as router out-of-the-box")
