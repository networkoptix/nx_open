from pprint import pformat

from netaddr import EUI, IPNetwork, mac_eui48
from winrm.exceptions import WinRMError

from framework.context_logger import ContextLogger, context_logger
from framework.method_caching import cached_property
from framework.networking.interface import Networking
from framework.os_access.windows_remoting import WinRM

_logger = ContextLogger(__name__, 'networking')


class PingError(Exception):
    def __init__(self, ip, message):
        super(PingError, self).__init__(message)
        self.ip = ip


@context_logger(_logger, 'framework.os_access.windows_remoting')
class WindowsNetworking(Networking):
    _firewall_rule_name = 'NX-TestStandNetwork'
    _firewall_rule_display_name = 'NX Test Stand Network'

    def __init__(self, winrm, macs):
        super(WindowsNetworking, self).__init__()
        self._names = {mac: 'Plugable {}'.format(slot) for slot, mac in macs.items()}
        self._winrm = winrm  # type: WinRM
        self.__repr__ = lambda: '<WindowsNetworking on {}>'.format(winrm)

    def rename_interfaces(self, mac_to_new_name):
        self._winrm.run_powershell_script(
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

    @cached_property  # TODO: Use cached_getter.
    def interfaces(self):
        self.rename_interfaces(self._names)
        return self._names

    def firewall_rule_exists(self):
        query = self._winrm.wmi_query(u'MSFT_NetFirewallRule', {}, namespace='Root/StandardCimv2')
        all_rules = list(query.enumerate())
        rules = [rule for rule in all_rules if rule[u'InstanceID'] == self._firewall_rule_name]
        return bool(rules)

    def create_firewall_rule(self):
        # No problem if there are multiple identical rules.
        query = self._winrm.wmi_query(u'MSFT_NetFirewallRule', {}, namespace='Root/StandardCimv2')
        properties_dict = {
            # See on numeric constants: https://msdn.microsoft.com/en-us/library/jj676843(v=vs.85).aspx
            u'InstanceID': self._firewall_rule_name,
            u'ElementName': self._firewall_rule_display_name,
            u'Direction': 2,  # Outbound.
            u'Action': 2,  # Allow.
            }
        try:
            selector_set = query.create(properties_dict)
        except WinRMError as e:
            if u'already exists' not in str(e):  # TODO: Retrieve detailed error from pywinrm internals.
                raise
            _logger.exception("Error when creating firewall rule.")
        else:
            self._winrm.run_powershell_script(
                # language=PowerShell
                '''
                    Set-NetFirewallRule -RemoteAddress @('10.0.0.0/8', '192.168.0.0/16') `
                        -Name:$Name
                    ''',
                {'Name': self._firewall_rule_name})

    def remove_firewall_rule(self):
        self._winrm.run_powershell_script(
            # language=PowerShell
            '''
                Remove-NetFirewallRule `
                    -Name:$Name `
                    -ErrorAction:SilentlyContinue `
                    -Confirm:$false
                ''',
            {'Name': self._firewall_rule_name})

    def _network_profile_wmi_query(self, informal_profile_name):
        query = self._winrm.wmi_query(
            u'MSFT_NetFirewallProfile',
            {u'InstanceID': u'MSFT|FW|FirewallProfile|{}'.format(informal_profile_name)},
            namespace='Root/StandardCimv2')
        return query

    def _allow_outbound_by_default(self, allow):
        value = 0 if allow else 1  # "0 -> enable, 1 or any positive -> disable.
        for profile_name in {'Private', 'Domain', 'Public'}:
            query = self._network_profile_wmi_query(profile_name)
            query.put({u'DefaultOutboundAction': value})

    def disable_internet(self):
        self._allow_outbound_by_default(False)

    def enable_internet(self):
        self._allow_outbound_by_default(True)

    def internet_is_enabled(self):
        profile_name = 'Private'  # Check only this: all interfaces assigned with the private profile.
        query = self._network_profile_wmi_query(profile_name)
        profile = query.get()
        return int(profile[u'DefaultOutboundAction']) == 0

    def setup_ip(self, mac, ip, prefix_length):
        all_configurations = self._winrm.wmi_query(u'Win32_NetworkAdapterConfiguration', {}).enumerate()
        requested_configuration, = [
            configuration for configuration in all_configurations
            if configuration[u'MACAddress'] != {u'@xsi:nil': u'true'} and EUI(configuration[u'MACAddress']) == mac]
        invoke_selectors = {u'Index': requested_configuration[u'Index']}
        invoke_query = self._winrm.wmi_query(u'Win32_NetworkAdapterConfiguration', invoke_selectors)
        subnet_mask = IPNetwork('{}/{}'.format(ip, prefix_length)).netmask
        invoke_arguments = {u'IPAddress': [str(ip)], u'SubnetMask': [str(subnet_mask)]}
        invoke_result = invoke_query.invoke_method(u'EnableStatic', invoke_arguments)
        if invoke_result[u'ReturnValue'] != u'0':
            raise RuntimeError('EnableStatic returned {}'.format(pformat(invoke_result)))

    def list_ips(self):
        result = self._winrm.run_powershell_script(
            # language=PowerShell
            '''
                Get-NetIPAddress -PolicyStore:PersistentStore -AddressFamily:IPv4 -ErrorAction:SilentlyContinue |
                    select InterfaceAlias,IPAddress
                    ''',
            {})
        return result

    def remove_ips(self):
        self._winrm.run_powershell_script(
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
        self._winrm.run_powershell_script(
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
        result = self._winrm.run_powershell_script(
            # language=PowerShell
            '''
                Get-NetRoute -PolicyStore:PersistentStore -AddressFamily:IPv4 -ErrorAction:SilentlyContinue |
                    select DestinationPrefix,InterfaceAlias,NextHop
                    ''',
            {})
        return result

    def remove_routes(self):
        self._winrm.run_powershell_script(
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

    def can_reach(self, ip, timeout_sec=4):
        query = self._winrm.wmi_query(u'Win32_PingStatus', {u'Address': str(ip)})
        status = query.get()
        return status[u'StatusCode'] == u'0'

    def reset(self):
        self.remove_routes()
        self.remove_ips()
        self.create_firewall_rule()

    def setup_nat(self, outer_mac):
        raise NotImplementedError("Windows 10 cannot be set up as router out-of-the-box")
