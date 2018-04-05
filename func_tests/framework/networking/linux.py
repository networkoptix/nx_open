import csv
import logging
from pprint import pformat

from netaddr import EUI
from pylru import lrudecorator

from framework.networking.interface import Networking
from framework.os_access import NonZeroExitStatus
from framework.utils import wait_until

logger = logging.getLogger(__name__)


class LinuxNetworking(Networking):
    def __init__(self, os_access, macs):
        super(LinuxNetworking, self).__init__()
        self._macs = macs
        self._os_access = os_access

    @property
    @lrudecorator(1)
    def interfaces(self):
        output = self._os_access.run_command(
            '''
            mkdir -p /tmp/func_tests/networking
            cd /tmp/func_tests/networking
            ls -1 /sys/class/net > interfaces.txt
            xargs -t -a interfaces.txt -I {} cat /sys/class/net/{}/address > macs.txt
            paste interfaces.txt macs.txt
            ''')
        interfaces = {
            EUI(raw_mac): interface
            for interface, raw_mac
            in csv.reader(output.splitlines(), delimiter='\t')
            if EUI(raw_mac) in self._macs}
        assert set(self._macs) == set(interfaces.keys())
        logger.info("Interfaces on %r:\n%s", self._os_access, pformat(interfaces))
        return interfaces

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
            iptables -A OUTPUT -d 192.168.0.0/16 -j ACCEPT
            iptables -A OUTPUT -j REJECT
            ''',
            env={'AVAILABLE_INTERFACES': '\n'.join(self.interfaces.values())})

    def setup_ip(self, mac, ip, prefix_length):
        interface = self.interfaces[mac]
        self._os_access.run_command(
            '''
            ip addr replace ${ADDRESS}/${PREFIX_LENGTH} dev ${INTERFACE} 
            ip link set dev ${INTERFACE} up
            ''',
            env={'INTERFACE': interface, 'ADDRESS': ip, 'PREFIX_LENGTH': prefix_length})
        logger.info("Machine %r has IP %s/%d on %s (%s).", self._os_access, ip, prefix_length, interface, mac)

    def route(self, destination_ip_net, gateway_bound_mac, gateway_ip):
        interface = self.interfaces[gateway_bound_mac]
        self._os_access.run_command(
            ['ip', 'route', 'replace', destination_ip_net, 'dev', interface, 'via', gateway_ip, 'proto', 'static'])

    def enable_internet(self):
        self._os_access.run_command(['iptables', '-D', 'OUTPUT', '-j', 'REJECT'])
        assert wait_until(lambda: self.can_reach('8.8.8.8'), name="until internet is on")

    def disable_internet(self):
        self._os_access.run_command(['iptables', '-A', 'OUTPUT', '-j', 'REJECT'])
        assert wait_until(lambda: not self.can_reach('8.8.8.8'), name="until internet is off")

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
        except NonZeroExitStatus as e:
            if e.exit_status == 1:  # See man page.
                return False
            raise
        return True
