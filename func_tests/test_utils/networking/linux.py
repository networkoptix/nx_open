import csv

from netaddr import EUI


class LinuxNodeNetworking(object):
    def __init__(self, os_access):
        self._os_access = os_access
        self._interface_names = dict()

        raw_mac_addresses = self._os_access.run_command('''
            mkdir -p /tmp/func_tests/networking && cd /tmp/func_tests/networking
            ls -1 /sys/class/net > interfaces.txt
            xargs -t -a interfaces.txt -I {} cat /sys/class/net/{}/address > mac_addresses.txt
            paste interfaces.txt mac_addresses.txt
            ''')
        self._interface_names = {
            EUI(mac_address): interface
            for interface, mac_address
            in csv.reader(raw_mac_addresses.splitlines(), delimiter='\t')}

    def reset(self, available_mac_addresses):
        """Don't touch localhost, host-bound interface and interfaces unknown to VM."""
        available_interfaces = {self._interface_names[mac_address] for mac_address in available_mac_addresses}
        self._os_access.run_command(
            '''
            echo "${AVAILABLE_INTERFACES}" | xargs -t -I {} ip addr flush dev {}
            echo "${AVAILABLE_INTERFACES}" | xargs -t -I {} ip link set dev {} down
            ip route flush proto static  # Manually routes are static.
            sysctl net.ipv4.ip_forward=0
            ''',
            env={'AVAILABLE_INTERFACES': '\n'.join(available_interfaces)})

    def setup_ip(self, mac_address, address, prefix_length):
        self._os_access.run_command(
            '''
                ip addr replace ${ADDRESS}/${PREFIX_LENGTH} dev ${INTERFACE} 
                ip link set dev ${INTERFACE} up
                ''',
            env={
                'INTERFACE': self._interface_names[mac_address],
                'ADDRESS': address,
                'PREFIX_LENGTH': prefix_length,
                })

    def route(self, destination_network, gateway_bound_mac_address, gateway_ip_address):
        self._os_access.run_command(
            'ip route replace ${NETWORK} dev ${INTERFACE} via ${GATEWAY} proto static',
            env={
                'INTERFACE': self._interface_names[gateway_bound_mac_address],
                'GATEWAY': gateway_ip_address,
                'NETWORK': destination_network,
                })

    def route_global(self, gateway_bound_mac_address, gateway_ip_address):
        self.route('default', gateway_bound_mac_address, gateway_ip_address)

    def prohibit_global(self):
        """Prohibit default route explicitly."""
        self._os_access.run_command('ip route replace prohibit default proto static')

    def setup_nat(self, outer_mac_address):
        """Connection can be initiated from inner_net_nodes only. Addresses are masqueraded."""
        self._os_access.run_command(
            '''
                sysctl net.ipv4.ip_forward=1
                iptables -t nat -A POSTROUTING -o ${OUTER_INTERFACE} -j MASQUERADE
                ''',
            env={'OUTER_INTERFACE': self._interface_names[outer_mac_address]})
