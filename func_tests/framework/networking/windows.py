#!/usr/bin/env python
import logging
from subprocess import check_output

from framework.windows_cmd_path import make_windows_cmd_path

log = logging.getLogger(__name__)


class VBox(object):
    def __init__(self, name, credentials):
        self._name = name
        self._credentials = credentials

    def _ps_commands(self):
        """
        ConvertTo-Json
        Get-NetConnectionProfile
        Get-WmiObject -Class Win32_NetworkAdapter
        Get-WmiObject -Class Win32_NetworkAdapterConfiguration
        Get-NetIPAddress
        Get-NetAdapter
        Get-NetAdapterAdvancedProperty
        Test-WSMan -ComputerName 10.5.0.10
        Winrm enumerate windows_remoting/config/listener
        winrs -r:10.5.0.10 -u:Administrator –p:qweasd123 ipconfig
        windows_remoting set windows_remoting/config/service/auth '@{Basic="true"}'
        windows_remoting set windows_remoting/config/service '@{AllowUnencrypted="true"}'
        """
        pass

    def get_property(self, property_name):
        output = check_output(['VBoxManage', 'guestproperty', 'get', self._name, property_name])
        for line in output.strip().splitlines():
            name, value = line.split(b': ', 1)
            if name == b'Value':
                return value.decode()
        assert False, "No 'Value' key in output:\n" + output.decode('ascii')


def assign_static_ip(connection, static_adapter_mac, static_address, prefix_length):
    adapters = connection.ps('gwmi', 'Win32_NetworkAdapter')  # Win32_NetworkAdapterConfiguration is OK too.
    static_adapter, = (
        adapter for adapter in adapters
        if adapter['MACAddress'] and adapter['MACAddress'].replace(':', '') == static_adapter_mac)
    connection.ps(
        'Remove-NetIPAddress',
        '-InterfaceIndex', str(static_adapter['InterfaceIndex']),
        '-Confirm:$false')
    connection.ps(
        'New-NetIPAddress',
        '-InterfaceIndex', str(static_adapter['InterfaceIndex']),
        '-IPAddress', static_address, '-PrefixLength', str(prefix_length))


def get_system_profile_path(connection):
    system_accounts = connection.ps('gwmi', 'Win32_SystemAccount')
    local_system, = (account for account in system_accounts if account['Name'] == 'SYSTEM')
    user_profiles = connection.ps('gwmi', 'Win32_UserProfile')
    local_system_profile, = (profile for profile in user_profiles if profile['SID'] == local_system['SID'])
    return local_system_profile['LocalPath']


def main():
    logging.basicConfig(level=logging.DEBUG)
    credentials = 'Administrator', 'qweasd123'
    vbox = VBox('wr', credentials)
    dynamic_ip = vbox.get_property('/VirtualBox/GuestInfo/Net/0/V4/IP')
    # temporary_connection = PowerShellConnection(dynamic_ip, credentials)
    # static_address = '10.6.0.100'
    # prefix_length = 24
    # static_adapter_mac = vbox.get_property('/VirtualBox/GuestInfo/Net/1/MAC')
    # assign_static_ip(temporary_connection, static_adapter_mac, static_address, prefix_length)
    # connection = PowerShellConnection(static_address, credentials)
    connection = WinRM(dynamic_ip, credentials)
    package_path = r'C:\Users\Administrator\Downloads\nxwitness-server-3.2.0.43-none-beta-test.exe'
    install_log_path = package_path + '.install.log'
    uninstall_log_path = package_path + '.uninstall.log'
    downloads = r'C:\Users\Administrator\Downloads'
    path = make_windows_cmd_path(connection.cmd, downloads)
    (path / 'oi.txt').write_text('hi\nhi\n\n\n\nwerwqerqwrwqr')
    print((path / 'oi.txt').read_text())

    # connection.cmd(package_path, '/uninstall', '/passive', '/l*x', install_log_path)
    # connection.cmd('type', install_log_path)
    # connection.cmd(package_path, '/passive', '/l*x', uninstall_log_path)
    # connection.cmd('type', uninstall_log_path)


if __name__ == '__main__':
    main()
