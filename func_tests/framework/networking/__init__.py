from itertools import islice
from uuid import uuid1

from netaddr import IPNetwork

from framework.utils import wait_until


def setup_networks(machines, hypervisor, networks_tree, reachability):
    """Assign IP addresses on all machines and setup NAT on router machines

    Doesn't know what machines are given. Used OS-agnostic interfaces only.
    Doesn't know whether machines are created and given as dict or are created as requested.

    :param machines: Dict or dynamic pool with .get(alias).
    :param hypervisor: Hypervisor interface, e.g. VirtualBox.
    :param networks_tree: Nested dict of specific format.
    :param reachability: Check what is expected to be reachable from what.
    :return: Machines which actually participated and their addresses.
    """

    # TODO: Consistent and sensible names.
    nodes_ips = {}
    allocated_machines = {}
    setup_uuid = uuid1()  # Network identifiers are unique to avoid clashes between tests and runs.

    def setup_tree(tree, router_alias, reachable_networks):
        for network_name in tree:
            network_uuid = '{} {}'.format(setup_uuid, network_name)
            network_ip = IPNetwork(network_name)
            ips = network_ip.iter_hosts()
            router_ip_address = next(ips)  # May be unused but must be reserved.
            nodes = dict(zip(tree[network_name].keys(), ips))
            if router_alias is not None:
                nodes[router_alias] = router_ip_address
            for alias, ip in nodes.items():
                try:
                    machine = allocated_machines[alias]
                except KeyError:
                    allocated_machines[alias] = machine = machines.get(alias)
                mac = hypervisor.plug(machine.name, network_uuid)
                machine.networking.setup_ip(mac, ip, network_ip.prefixlen)
                if alias != router_alias:
                    # Routes on router was set up when it was interpreted as host.
                    for reachable_network in reachable_networks:
                        machine.networking.route(reachable_network, mac, router_ip_address)
                    # Default gateway can be specified explicitly.
                    subtree = tree[network_name][alias]
                    if subtree is not None:
                        machine.networking.setup_nat(mac)
                        setup_tree(subtree, alias, reachable_networks + [network_ip])
            for alias, ip in nodes.items():
                nodes_ips.setdefault(alias, {})[network_ip] = ip

    setup_tree(networks_tree, None, [])

    for alias, ips in nodes_ips.items():
        for ip in ips.values():
            assert wait_until(
                lambda: allocated_machines[alias].networking.can_reach(ip, timeout_sec=1),
                name="until {} can ping itself by {}".format(alias, ip),
                timeout_sec=20)
    for destination_net in reachability:
        for destination_alias in reachability[destination_net]:
            for source_alias in reachability[destination_net][destination_alias]:
                destination_ip = nodes_ips[destination_alias][IPNetwork(destination_net)]
                assert wait_until(
                    lambda: allocated_machines[source_alias].networking.can_reach(destination_ip, timeout_sec=1),
                    name="until {} can ping {} by {}".format(source_alias, destination_alias, destination_ip),
                    timeout_sec=60)

    return allocated_machines, nodes_ips


def setup_flat_network(machines, network_ip, hypervisor):  # TODO: Use in setup networks.
    network_uuid = '{} {} flat'.format(uuid1(), network_ip)
    iter_ips = network_ip.iter_hosts()
    next(iter_ips)  # First IP is usually reserved for router.
    host_ips = dict(zip((machine.alias for machine in machines), iter_ips))
    for machine in machines:
        mac = hypervisor.plug(machine.name, network_uuid)
        machine.networking.setup_ip(mac, host_ips[machine.alias], network_ip.prefixlen)
    return host_ips
