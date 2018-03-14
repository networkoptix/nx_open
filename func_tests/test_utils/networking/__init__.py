from netaddr import IPNetwork

from test_utils.utils import wait_until


def setup_networks(machines, hypervisor, networks_tree, reachability):
    nodes_ips = {}
    allocated_machines = {}

    def setup_tree(tree, router_alias, reachable_networks):
        for network in tree:
            ip_net = IPNetwork(network)  # Network name is same as network address space.
            ips = ip_net.iter_hosts()
            router_ip_address = next(ips)  # May be unused but must be reserved.
            nodes = dict(zip(tree[network].keys(), ips))
            if router_alias is not None:
                nodes[router_alias] = router_ip_address
            for alias, ip in nodes.items():
                machine = machines['linux'].get(alias)  # TODO: Specify OS (generally, VM type) of interest.
                allocated_machines[alias] = machine
                mac = hypervisor.plug(machine.name, network)
                machine.networking.setup_ip(mac, ip, ip_net.prefixlen)
                if alias != router_alias:
                    # Routes on router was set up when it was interpreted as host.
                    for reachable_network in reachable_networks:
                        machine.networking.route(reachable_network, mac, router_ip_address)
                    # Default gateway can be specified explicitly.
                    subtree = tree[network][alias]
                    if subtree is not None:
                        machine.networking.setup_nat(mac)
                        setup_tree(subtree, alias, reachable_networks + [ip_net])
            for alias, ip in nodes.items():
                nodes_ips.setdefault(alias, {})[ip_net] = ip

    setup_tree(networks_tree, None, [])

    for alias, ips in nodes_ips.items():
        for ip in ips.values():
            assert wait_until(lambda: allocated_machines[alias].networking.can_reach(ip, 1))
    for destination_net in reachability:
        for destination_alias in reachability[destination_net]:
            for source_alias in reachability[destination_net][destination_alias]:
                destination_ip = nodes_ips[destination_alias][IPNetwork(destination_net)]
                assert wait_until(lambda: allocated_machines[source_alias].networking.can_reach(destination_ip, 1))

    return allocated_machines, nodes_ips
