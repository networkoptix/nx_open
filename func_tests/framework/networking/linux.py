import csv
import logging
from pprint import pformat

from netaddr import EUI

from framework.method_caching import cached_property
from framework.networking.interface import Networking
from framework.os_access.exceptions import NonZeroExitStatus, exit_status_error_cls
from framework.os_access.posix_shell import SSH
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)  # TODO: Rename all such vars to `_logger`.


class LinuxNetworking(Networking):
    def __init__(self, ssh, macs):
        super(LinuxNetworking, self).__init__()
        self._macs = macs
        self._ssh = ssh  # type: SSH

    @cached_property  # TODO: Use cached_getter.
    def interfaces(self):
        output = self._ssh.run_sh_script(
            # language=Bash
            '''
                mkdir -p /tmp/func_tests/networking
                cd /tmp/func_tests/networking
                ls -1 /sys/class/net >interfaces.txt
                xargs -t -a interfaces.txt -I '{}' cat '/sys/class/net/{}/address' >macs.txt
                paste interfaces.txt macs.txt
                ''')
        mac_values = set(self._macs.values())
        interfaces = {
            EUI(raw_mac): interface
            for interface, raw_mac
            in csv.reader(output.splitlines(), delimiter='\t')
            if EUI(raw_mac) in mac_values}
        assert mac_values == set(interfaces.keys())
        _logger.info("Interfaces on %r:\n%s", self._ssh, pformat(interfaces))
        return interfaces

    def reset(self):
        """Don't touch localhost, host-bound interface and interfaces unknown to Machine."""
        self._ssh.run_sh_script(
            # language=Bash
            '''
                echo "${AVAILABLE_INTERFACES}" | xargs -t -I '{}' ip addr flush dev '{}'
                echo "${AVAILABLE_INTERFACES}" | xargs -t -I '{}' ip link set dev '{}' down
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
        self._ssh.run_sh_script(
            # language=Bash
            '''
                ip addr replace ${ADDRESS}/${PREFIX_LENGTH} dev ${INTERFACE}
                ip link set dev ${INTERFACE} up
                ''',
            env={'INTERFACE': interface, 'ADDRESS': ip, 'PREFIX_LENGTH': prefix_length})
        _logger.info("Machine %r has IP %s/%d on %s (%s).", self._ssh, ip, prefix_length, interface, mac)

    def route(self, destination_ip_net, gateway_bound_mac, gateway_ip):
        interface = self.interfaces[gateway_bound_mac]
        self._ssh.run_command(
            ['ip', 'route', 'replace', destination_ip_net, 'dev', interface, 'via', gateway_ip, 'proto', 'static'])

    def enable_internet(self):
        while True:
            try:
                self._ssh.run_command(['iptables', '-D', 'OUTPUT', '-j', 'REJECT'])
            except NonZeroExitStatus:
                _logger.debug("No more internet restricting rules in iptables.")
                break
        global_ip = '8.8.8.8'
        wait_for_true(
            lambda: self.can_reach(global_ip),
            "internet on {} is on ({} is reachable)".format(self, global_ip))

    def disable_internet(self):
        try:
            self._ssh.run_command(['iptables', '-C', 'OUTPUT', '-j', 'REJECT'])
        except NonZeroExitStatus:
            self._ssh.run_command(['iptables', '-A', 'OUTPUT', '-j', 'REJECT'])
        global_ip = '8.8.8.8'
        wait_for_true(
            lambda: not self.can_reach(global_ip),
            "internet on {} is off ({} is unreachable)".format(self, global_ip))

    def setup_nat(self, outer_mac):
        """Connection can be initiated from inner_net_nodes only. Addresses are masqueraded."""
        self._ssh.run_sh_script(
            # language=Bash
            '''
                sysctl net.ipv4.ip_forward=1
                iptables -t nat -A POSTROUTING -o ${OUTER_INTERFACE} -j MASQUERADE
                ''',
            env={'OUTER_INTERFACE': self.interfaces[outer_mac]})

    def can_reach(self, ip, timeout_sec=4):
        try:
            self._ssh.run_command(['ping', '-c', 1, '-W', timeout_sec, ip])
        except exit_status_error_cls(1):  # See man page.
            return False
        return True
