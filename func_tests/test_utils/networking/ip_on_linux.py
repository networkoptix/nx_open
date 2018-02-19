import csv

from netaddr import EUI


class LinuxIpNode(object):
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

    def reset_everything(self, host_only_mac_address, other_mac_addresses):
        self._os_access.run_command(
            '''
            echo "${OTHER_INTERFACES}" | xargs -t -I {} ip addr flush dev {}
            echo "${OTHER_INTERFACES}" | xargs -t -I {} ip link set dev {} down
            ip route flush proto static  # All manual routes were made static.
            sysctl net.ipv4.ip_forward=0
            iptables -P INPUT ACCEPT
            iptables -P OUTPUT ACCEPT
            iptables -P FORWARD ACCEPT
            iptables -F
            iptables -t nat -F
            iptables -A OUTPUT -o ${HOST_INTERFACE} -m state --state RELATED,ESTABLISHED -j ACCEPT
            iptables -X
            iptables -N CONTROL_INTERNET
            iptables -A CONTROL_INTERNET -j REJECT
            iptables -A OUTPUT -o ${HOST_INTERFACE} -j CONTROL_INTERNET
            iptables -A INPUT -i ${HOST_INTERFACE} -p tcp --dport 22 -j ACCEPT  # SSH.
            iptables -A INPUT -i ${HOST_INTERFACE} -p tcp --dport 7001 -j ACCEPT  # VMS.
            iptables -A INPUT -i ${HOST_INTERFACE} -p icmp -j ACCEPT  # Allow incoming pings and ping responses.
            iptables -A INPUT -i ${HOST_INTERFACE} -j CONTROL_INTERNET
            ''',
            env={
                'OTHER_INTERFACES': '\n'.join(
                    self._interface_names[mac_address]
                    for mac_address in other_mac_addresses),  # Flush what VM knows, not localhost.
                'HOST_INTERFACE': self._interface_names[host_only_mac_address]
                })

    def set_address(self, mac_address, ip_address, ip_network):
        self._os_access.run_command(
            '''
                ip addr replace ${IP_ADDRESS}/${PREFIX_LENGTH} dev ${INTERFACE} 
                ip link set dev ${INTERFACE} up
                ''',
            env={
                'INTERFACE': self._interface_names[mac_address],
                'IP_ADDRESS': ip_address,
                'PREFIX_LENGTH': ip_network.prefixlen
                })

    def add_routes(self, ip_network, router_ip_address):
        self._os_access.run_command(
            'ip route replace ${IP_NETWORK} via ${ROUTER_IP_ADDRESS} proto static',
            env={'ROUTER_IP_ADDRESS': router_ip_address, 'IP_NETWORK': ip_network})

    def set_default_gateway(self, interface_mac_address, gateway_ip_address):
        self._os_access.run_command(
            'ip route replace default dev ${INTERFACE} via ${GATEWAY} proto static',
            env={
                'INTERFACE': self._interface_names[interface_mac_address],
                'GATEWAY': gateway_ip_address})

    def delete_default_gateway(self):
        """Prohibit default route explicitly."""
        self._os_access.run_command('ip route replace prohibit default proto static')

    def enable_masquerading(self, outer_mac_address):
        """Connection can be initiated from inner_net_nodes only. Addresses are masqueraded."""
        self._os_access.run_command(
            '''
                sysctl net.ipv4.ip_forward=1
                iptables -t nat -A POSTROUTING -o ${OUTER_INTERFACE} -j MASQUERADE
                ''',
            env={'OUTER_INTERFACE': self._interface_names[outer_mac_address]})

    def enable_internet(self, host_nat_interface_mac_address, host_ip_address):
        self._os_access.run_command(
            '''
            ip route replace default dev ${INTERFACE} via ${GATEWAY} proto static
            iptables -D CONTROL_INTERNET -j REJECT
            iptables -A CONTROL_INTERNET -j ACCEPT
            ''',
            env={
                'INTERFACE': self._interface_names[host_nat_interface_mac_address],
                'GATEWAY': host_ip_address})
