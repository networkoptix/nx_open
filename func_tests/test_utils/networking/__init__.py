from netaddr import IPNetwork


class NodeNetworking(object):
    def __init__(self, os_networking, hypervisor_networking):
        self.os = os_networking
        self.hypervisor = hypervisor_networking

    def route_global_through_host(self):
        self.hypervisor.os.route_global(self.hypervisor.available_mac_addresses)

    def reset(self):
        self.hypervisor.unplug_all()
        self.os.flush_ip(self.hypervisor.available_mac_addresses)


def setup_networks(machines, networks_tree, default_gateways=None, ):
    default_gateways = default_gateways or {}
    unassigned_machines = machines
    assigned_machines = dict()  # Alias from structure -> machine.

    def setup_subtree(subtree, router_alias=None, outer_networks=None):
        outer_networks = outer_networks or []
        for network_name in subtree:
            network = IPNetwork(network_name)  # Network name is same as network address space.
            network_ip_addresses = network.iter_hosts()
            router_ip_address = next(network_ip_addresses)  # May be unused but must be reserved.
            if router_alias is not None:  # No router on top level.
                router_node = assigned_machines[router_alias].networking  # Added when processed as host.
                router_mac_address = router_node.hypervisor.plug(network_name)
                router_node.os.setup_ip(router_mac_address, router_ip_address, network.prefixlen)
            for host_alias in sorted(subtree[network_name].keys()):
                try:
                    host_networking = assigned_machines[host_alias].networking
                except KeyError:
                    assigned_machines[host_alias] = unassigned_machines.pop()
                    host_networking = assigned_machines[host_alias].networking
                host_mac_address = host_networking.hypervisor.plug(network_name)  # Connected to this network.
                host_ip_address = next(network_ip_addresses)  # Connected to this network.
                host_networking.os.setup_ip(host_mac_address, host_ip_address, network.prefixlen)
                for outer_network in outer_networks:
                    host_networking.os.route(outer_network, host_mac_address, router_ip_address)
                if router_alias is not None:
                    # Disambiguate default gateway explicitly if host is connected to multiple networks.
                    if host_alias not in default_gateways or default_gateways[host_alias] == router_alias:
                        host_networking.os.route_global(host_mac_address, router_ip_address)
                if subtree[network_name][host_alias] is not None:
                    host_networking.os.enable_masquerading(host_mac_address)  # Router for underlying subnet.
                    setup_subtree(subtree[network_name][host_alias], host_alias, outer_networks + [network])

    setup_subtree(networks_tree)

    for alias in default_gateways:
        networking = assigned_machines[alias].networking
        if default_gateways[alias] is None:
            networking.os.prohibit_global()

    return assigned_machines, unassigned_machines
