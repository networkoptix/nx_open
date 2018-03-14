from netaddr import IPNetwork


def setup_networks(machines, hypervisor, networks_tree):
    nodes_ip_addresses = {}
    allocated_machines = {'linux': {}}

    def setup_tree(tree, router_alias, reachable_networks):
        for network_name in tree:
            network = IPNetwork(network_name)  # Network name is same as network address space.
            ip_addresses = network.iter_hosts()
            router_ip_address = next(ip_addresses)  # May be unused but must be reserved.
            nodes = dict(zip(tree[network_name].keys(), ip_addresses))
            if router_alias is not None:
                nodes[router_alias] = router_ip_address
            for alias, ip_address in nodes.items():
                machine = machines['linux'].get(alias)  # TODO: Specify OS (generally, VM type) of interest.
                allocated_machines[alias] = machine
                mac_address = hypervisor.plug(machine.info.name, network_name)
                machine.networking.setup_ip(mac_address, ip_address, network.prefixlen)
                if alias != router_alias:
                    # Routes on router was set up when it was interpreted as host.
                    for reachable_network in reachable_networks:
                        machine.networking.route(reachable_network, mac_address, router_ip_address)
                    # Default gateway can be specified explicitly.
                    subtree = tree[network_name][alias]
                    if subtree is not None:
                        machine.networking.setup_nat(mac_address)
                        setup_tree(subtree, alias, reachable_networks + [network])
            for alias, ip_address in nodes.items():
                nodes_ip_addresses.setdefault(alias, []).append(ip_address)

    setup_tree(networks_tree, None, [])

    return allocated_machines
