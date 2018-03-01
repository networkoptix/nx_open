from collections import namedtuple

from netaddr import IPNetwork

NodeNetworking = namedtuple('NodeNetworking', ['os_networking', 'hypervisor_networking'])


def enable_internet(machine):
    machine.networking.os_networking.route_global(
        machine.networking.hypervisor_networking.host_bound_mac_address,
        machine.networking.hypervisor_networking.host_ip_address)


def disable_internet(machine):
    machine.networking.os_networking.prohibit_global()


def reset_networking(machine):
    machine.networking.hypervisor_networking.unplug_all()
    machine.networking.os_networking.reset(machine.networking.hypervisor_networking.available_mac_addresses)
    enable_internet(machine)


def setup_networks(machine_factory, networks_tree, default_gateways=None):
    default_gateways = default_gateways or {}
    nodes_ip_addresses = {}

    def setup_tree(tree, router_alias, reachable_networks):
        for network_name in tree:
            network = IPNetwork(network_name)  # Network name is same as network address space.
            ip_addresses = network.iter_hosts()
            router_ip_address = next(ip_addresses)  # May be unused but must be reserved.
            nodes = dict(zip(tree[network_name].keys(), ip_addresses))
            if router_alias is not None:
                nodes[router_alias] = router_ip_address
            for alias, ip_address in nodes.items():
                networking = machine_factory.get(alias).networking
                mac_address = networking.hypervisor_networking.plug(network_name)
                networking.os_networking.setup_ip(mac_address, ip_address, network.prefixlen)
                if alias != router_alias:
                    # Routes on router was set up when it was interpreted as host.
                    for reachable_network in reachable_networks:
                        networking.os_networking.route(reachable_network, mac_address, router_ip_address)
                    # Default gateway can be specified explicitly.
                    if alias in default_gateways and default_gateways[alias] == router_alias:
                        networking.os_networking.route_global(mac_address, router_ip_address)
                    subtree = tree[network_name][alias]
                    if subtree is not None:
                        networking.os_networking.setup_nat(mac_address)
                        setup_tree(subtree, alias, reachable_networks + [network])
            for alias, ip_address in nodes.items():
                nodes_ip_addresses.setdefault(alias, []).append(ip_address)

    setup_tree(networks_tree, None, [])

    for alias in default_gateways:
        if default_gateways[alias] is None:
            machine_factory.get(alias).networking.os_networking.prohibit_global()
