'''Virtual Box VBoxManage command wrapper'''

import re
from netaddr import IPNetwork
from .host import Host


INTERNAL_NETWORK_NAME_PREFIX = 'funtest-net-'


def make_network(ip_address, prefixlen):
    network = IPNetwork(ip_address)
    network.prefixlen = prefixlen
    return network

def funtest_dhcp_server(name, network):
    return DhcpServer(
        name=name,
        ip=make_network(network[2], network.prefixlen),  # .2 will be our dhcp server
        lower_ip_address=make_network(network[101], network.prefixlen),
        upper_ip_address=make_network(network[254], network.prefixlen),
        enabled=True,
        )


class DhcpServer(object):

    def __init__(self, name=None, ip=None, lower_ip_address=None, upper_ip_address=None, enabled=None):
        self.name = name
        self.ip = ip  # IPNetwork
        self.lower_ip_address = lower_ip_address  # IPNetwork
        self.upper_ip_address = upper_ip_address  # IPNetwork
        self.enabled = enabled

    def network_matches(self, network):
        return self.ip.network == network.network

    def properties_match(self, other):
        assert isisntance(other, DhcpServer), repr(other)
        return (self.lower_ip_address == other.lower_ip_address
                and self.upper_ip_address == other.upper_ip_address
                and self.enabled == other.enabled)


class VBoxManage(object):

    def __init__(self, host):
        assert isinstance(host, Host), repr(host)
        self._host = host
        self._dhcp_server_list, self._dhcp_net_index = self._load_dhcp_server_list()

    def does_vms_exist(self, vms_name):
        output = self._run_command(['list', 'vms'])
        vms_list = [line.split()[0].strip('"') for line in output.splitlines()]
        return vms_name in vms_list

    def get_vms_state(self, vms_name):
        output = self._run_command(['showvminfo', vms_name, '--machinereadable'])
        properties = {}
        for line in output.splitlines():
            name, value = line.split('=')
            properties[name] = value.strip('"')
        return properties['VMState']

    def poweroff_vms(self, vms_name):
        self._run_command(['controlvm', vms_name, 'poweroff'])

    def delete_vms(self, vms_name):
        self._run_command(['unregistervm', '--delete', vms_name])

    def produce_internal_network(self, network):
        l = [dhcp_server for dhcp_server in self._dhcp_server_list
             if dhcp_server.network_matches(network)]
        if l:
            return l[0].name
        self._dhcp_net_index += 1
        name = INTERNAL_NETWORK_NAME_PREFIX + str(self._dhcp_net_index)
        dhcp_server = funtest_dhcp_server(name, network)
        self._add_dhcp_server(dhcp_server)
        self._dhcp_server_list.append(dhcp_server)
        return name

    def _load_dhcp_server_list(self):
        output = self._run_command(['list', 'dhcpservers'])
        servers = []
        last_index = 0
        for server_output in output.split('\n\n'):
            server = DhcpServer()
            ip = None
            if not server_output:
                continue
            for line in server_output.splitlines():
                name, value = re.split(r':\s+', line)
                name = name.lower()
                if name == 'networkname':
                    server.name = value
                elif name == 'ip':
                    ip = value
                elif name == 'networkmask':
                    assert ip
                    server.ip = IPNetwork('%s/%s' % (ip, value))
                elif name == 'loweripaddress':
                    assert server.ip
                    server.lower_ip_address = make_network(value, server.ip.prefixlen)
                elif name == 'upperipaddress':
                    assert server.ip
                    server.upper_ip_address = make_network(value, server.ip.prefixlen)
                elif name == 'enabled':
                    assert value in ['Yes', 'No'], repr(value)
                    server.enabled = value == 'Yes'
                else:
                    assert False, 'VBoxManage list dhcpservers returned unknown dhcp server attribute: %r' % name
            if server.name.startswith('HostInterfaceNetworking-'):
                continue  # skip dhcp servers for host-only networks
            servers.append(server)
            if server.name.startswith(INTERNAL_NETWORK_NAME_PREFIX):
                last_index = max(last_index, int(server.name[len(INTERNAL_NETWORK_NAME_PREFIX):]))
        return (servers, last_index)

    def _add_dhcp_server(self, dhcp_server):
        self._run_command(['dhcpserver', 'add',
                           '--netname', dhcp_server.name,
                           '--ip', dhcp_server.ip.ip,
                           '--netmask', dhcp_server.ip.netmask,
                           '--lowerip', dhcp_server.lower_ip_address.ip,
                           '--upperip', dhcp_server.upper_ip_address.ip,
                           '--enable',
                           ])

    def _run_command(self, args):
        return self._host.run_command(['VBoxManage', '--nologo'] + args)
