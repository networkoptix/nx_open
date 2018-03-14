import csv
import logging

from netaddr import EUI

from test_utils.os_access import ProcessError

logger = logging.getLogger(__name__)


class LinuxNetworking(object):
    def __init__(self, os_access, macs):
        self._os_access = os_access
        output = self._os_access.run_command(
            '''
            mkdir -p /tmp/func_tests/networking
            cd /tmp/func_tests/networking
            ls -1 /sys/class/net > interfaces.txt
            xargs -t -a interfaces.txt -I {} cat /sys/class/net/{}/address > macs.txt
            paste interfaces.txt macs.txt
            ''')
        self.interfaces = {
            EUI(raw_mac): interface
            for interface, raw_mac
            in csv.reader(output.splitlines(), delimiter='\t')
            if EUI(raw_mac) in macs}

    def reset(self):
        """Don't touch localhost, host-bound interface and interfaces unknown to VM."""
        self._os_access.run_command(
            '''
            echo "${AVAILABLE_INTERFACES}" | xargs -t -I {} ip addr flush dev {}
            echo "${AVAILABLE_INTERFACES}" | xargs -t -I {} ip link set dev {} down
            ip route flush proto static  # Manually added routes are static.
            sysctl net.ipv4.ip_forward=0
            iptables -F
            iptables -t nat -F
            iptables -A OUTPUT -m state --state RELATED,ESTABLISHED -j ACCEPT
            iptables -A OUTPUT -o lo -j ACCEPT
            iptables -A OUTPUT -d 10.0.0.0/8 -j ACCEPT
            iptables -A OUTPUT -j REJECT
            ''',
            env={'AVAILABLE_INTERFACES': '\n'.join(self.interfaces.values())})

    def setup_ip(self, mac, ip, prefix_length):
        self._os_access.run_command(
            '''
            ip addr replace ${ADDRESS}/${PREFIX_LENGTH} dev ${INTERFACE} 
            ip link set dev ${INTERFACE} up
            ''',
            env={'INTERFACE': self.interfaces[mac], 'ADDRESS': ip, 'PREFIX_LENGTH': prefix_length})

    def route(self, destination_ip_net, gateway_bound_mac, gateway_ip):
        interface = self.interfaces[gateway_bound_mac]
        self._os_access.run_command(
            ['ip', 'route', 'replace', destination_ip_net, 'dev', interface, 'via', gateway_ip, 'proto', 'static'])

    def enable_internet(self):
        self._os_access.run_command(['iptables', '-D', 'OUTPUT', '-j', 'REJECT'])
        assert self.can_reach('8.8.8.8')

    def disable_internet(self):
        self._os_access.run_command(['iptables', '-A', 'OUTPUT', '-j', 'REJECT'])
        assert not self.can_reach('8.8.8.8')

    def setup_nat(self, outer_mac):
        """Connection can be initiated from inner_net_nodes only. Addresses are masqueraded."""
        self._os_access.run_command(
            '''
            sysctl net.ipv4.ip_forward=1
            iptables -t nat -A POSTROUTING -o ${OUTER_INTERFACE} -j MASQUERADE
            ''',
            env={'OUTER_INTERFACE': self.interfaces[outer_mac]})

    def can_reach(self, ip, timeout_sec=4):
        try:
            self._os_access.run_command(['ping', '-c', 1, '-W', timeout_sec, ip])
        except ProcessError as e:
            if e.returncode == 1:  # See man page.
                return False
            raise
        return True
