from netaddr import IPNetwork

from test_utils.networking import link_on_virtualbox as link  # Static polymorphism.


def setup_networks(ip_nodes, networks, default_gateways):
    unassigned_vm_names = set(ip_nodes.keys())
    assigned_vm_names = dict()

    # Always disconnect all nodes.
    for name, node in ip_nodes.items():
        node.reset_everything(link.host_nat_interface_mac_address(name), link.other_mac_addresses(name))
        link.unplug_all(name)

    def setup_subtree(subtree, router_alias, outer_ip_networks):
        for network_alias in subtree:
            ip_network = IPNetwork(network_alias)  # Alias is same as network address space.
            ip_addresses = ip_network.iter_hosts()
            router_ip_address = next(ip_addresses)  # May be unused but must be reserved.
            if router_alias is not None:  # No router on top level.
                router_vm_name = assigned_vm_names[router_alias]  # Added when processed as host.
                router_mac_address = link.plug(router_vm_name, network_alias)
                ip_nodes[router_vm_name].set_address(router_mac_address, router_ip_address, ip_network)
            for alias in sorted(subtree[network_alias].keys()):
                if alias not in assigned_vm_names:  # TODO: Names in config should be same as VM names.
                    assigned_vm_names[alias] = unassigned_vm_names.pop()
                vm_name = assigned_vm_names[alias]
                mac_address = link.plug(vm_name, network_alias)
                ip_address = next(ip_addresses)
                ip_nodes[vm_name].set_address(mac_address, ip_address, ip_network)
                for outer_ip_network in outer_ip_networks:
                    ip_nodes[vm_name].add_routes(outer_ip_network, router_ip_address)
                if router_alias is not None:
                    # Disambiguate default gateway explicitly if host is connected to multiple networks.
                    if alias not in default_gateways or default_gateways[alias] == router_alias:
                        ip_nodes[vm_name].set_default_gateway(mac_address, router_ip_address)
                if subtree[network_alias][alias] is not None:
                    ip_nodes[vm_name].enable_masquerading(mac_address)  # Host is router for underlying NAT subnet.
                    setup_subtree(subtree[network_alias][alias], alias, outer_ip_networks + [ip_network])

    setup_subtree(networks, None, [])

    for name in default_gateways:
        vm_name = assigned_vm_names[name]
        if default_gateways[name] is None:
            ip_nodes[vm_name].delete_default_gateway()
        elif default_gateways[name] == 'internet':  # Special.
            ip_nodes[vm_name].enable_internet(link.host_nat_interface_mac_address(vm_name), link.host_ip_address())

    return assigned_vm_names, unassigned_vm_names
