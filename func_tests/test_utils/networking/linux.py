import csv
import logging

from netaddr import EUI

logger = logging.getLogger(__name__)


class LinuxNetworking(object):
    def __init__(self, os_access, available_macs):
        self._os_access = os_access
        output = self._os_access.run_command(
            '''
            mkdir -p /tmp/func_tests/networking
            cd /tmp/func_tests/networking
            ls -1 /sys/class/net > interfaces.txt
            xargs -t -a interfaces.txt -I {} cat /sys/class/net/{}/address > macs.txt
            paste interfaces.txt macs.txt
            ''')
        interfaces = {EUI(mac): interface for interface, mac in csv.reader(output.splitlines(), delimiter='\t')}
        self.available_interfaces = {interfaces[mac]: mac for mac in available_macs}

    def reset(self):
        """Don't touch localhost, host-bound interface and interfaces unknown to VM."""
        self._os_access.run_command(
            '''
            echo "${AVAILABLE_INTERFACES}" | xargs -t -I {} ip addr flush dev {}
            echo "${AVAILABLE_INTERFACES}" | xargs -t -I {} ip link set dev {} down
            ip route flush proto static  # Manually routes are static.
            sysctl net.ipv4.ip_forward=0
            iptables -F
            iptables -t nat -F
            iptables -A OUTPUT -m state --state RELATED,ESTABLISHED -j ACCEPT
            iptables -A OUTPUT -o lo -j ACCEPT
            iptables -A OUTPUT -d 10.0.0.0/8 -j ACCEPT
            ''',
            env={'AVAILABLE_INTERFACES': '\n'.join(self.available_interfaces.keys())})

    def setup_ip(self, mac, ip, prefix_length):
        self._os_access.run_command(
            '''
            ip addr replace ${ADDRESS}/${PREFIX_LENGTH} dev ${INTERFACE} 
            ip link set dev ${INTERFACE} up
            ''',
            env={'INTERFACE': self.available_interfaces[mac], 'ADDRESS': ip, 'PREFIX_LENGTH': prefix_length})

    def route(self, destination_ip_net, gateway_bound_mac, gateway_ip):
        interface = self.available_interfaces[gateway_bound_mac]
        self._os_access.run_command(
            ['ip', 'route', 'replace', destination_ip_net, 'dev', interface, 'via', gateway_ip, 'proto', 'static'])

    def enable_internet(self):
        self._os_access.run_command(['iptables', '-D', 'OUTPUT', '-j', 'REJECT'])

    def disable_internet(self):
        self._os_access.run_command(['iptables', '-A', 'OUTPUT', '-j', 'REJECT'])

    def setup_nat(self, outer_mac):
        """Connection can be initiated from inner_net_nodes only. Addresses are masqueraded."""
        self._os_access.run_command(
            '''
            sysctl net.ipv4.ip_forward=1
            iptables -t nat -A POSTROUTING -o ${OUTER_INTERFACE} -j MASQUERADE
            ''',
            env={'OUTER_INTERFACE': self.available_interfaces[outer_mac]})
