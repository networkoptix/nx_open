from netaddr import IPAddress, IPNetwork
from typing import Any, Iterable, Mapping

from framework.vms.hypervisor.hypervisor import Hypervisor
from framework.vms.vm_type import VM
from framework.waiting import wait_for_truthy


def _assign_ips(network_ip, host_aliases):
    # type: (IPNetwork, Iterable[str]) -> (IPNetwork, Mapping[str, IPNetwork])
    """Router IP address is always reserved, whether router is actually present or not."""
    ip_iter = iter(
        IPNetwork((ip_address.value, network_ip.prefixlen))
        for ip_address in network_ip.iter_hosts())
    router_ip = next(ip_iter)
    host_ips = dict(zip(host_aliases, ip_iter))
    return router_ip, host_ips


def setup_networks(machines, hypervisor, networks_tree, reachability):
    # type: (Mapping[VM], Hypervisor, Any, Any) -> Mapping[str, Mapping[IPNetwork, IPAddress]]
    """Assign IP addresses on all machines and setup NAT on router machines

    Doesn't know what machines are given. Used OS-agnostic interfaces only.
    Doesn't know whether machines are created and given as dict or are created as requested.

    :param machines: Callable to allocate a machine.
    :param hypervisor: Hypervisor interface, e.g. VirtualBox.
    :param networks_tree: Nested dict of specific format.
    :param reachability: Check what is expected to be reachable from what.
    :return: IP addresses.
    """

    # TODO: Consistent and sensible names.
    node_ip_addresses = {}

    def setup_tree(tree, router_alias, reachable_networks):
        for network_name in tree:
            network_uuid = hypervisor.make_internal_network(network_name)
            network_ip = IPNetwork(network_name)
            router_ip, nodes = _assign_ips(network_ip, tree[network_name].keys())
            if router_alias is not None:
                nodes[router_alias] = router_ip
            for alias, ip in nodes.items():
                machine = machines[alias]
                mac = machine.hardware.plug_internal(network_uuid)
                machine.os_access.networking.setup_network(mac, ip)
                if alias != router_alias:
                    # Routes on router was set up when it was interpreted as host.
                    for reachable_network in reachable_networks:
                        machine.os_access.networking.route(reachable_network, mac, router_ip.ip)
                    # Default gateway can be specified explicitly.
                    subtree = tree[network_name][alias]
                    if subtree is not None:
                        machine.os_access.networking.setup_nat(mac)
                        setup_tree(subtree, alias, reachable_networks + [network_ip])
            for alias, ip in nodes.items():
                node_ip_addresses.setdefault(alias, {})[network_ip] = ip.ip

    setup_tree(networks_tree, None, [])

    for alias, ip_addresses in node_ip_addresses.items():
        for ip_address in ip_addresses.values():
            wait_for_truthy(
                lambda: machines[alias].os_access.networking.can_reach(ip_address, timeout_sec=1),
                "machine {} can reach itself by {}".format(alias, ip_address),
                timeout_sec=20)
    for destination_net in reachability:
        for destination_alias in reachability[destination_net]:
            for source_alias in reachability[destination_net][destination_alias]:
                destination_ip = node_ip_addresses[destination_alias][IPNetwork(destination_net)]
                wait_for_truthy(
                    lambda: machines[source_alias].os_access.networking.can_reach(destination_ip, timeout_sec=1),
                    "machine {} can reach {} by {}".format(source_alias, destination_alias, destination_ip),
                    timeout_sec=60)

    return node_ip_addresses


def setup_flat_network(machines, network_ip, hypervisor):
    network_uuid = hypervisor.make_internal_network(str(network_ip))
    _, host_ips = _assign_ips(network_ip, (machine.alias for machine in machines))
    for machine in machines:
        mac = machine.hardware.plug_internal(network_uuid)
        ip = host_ips[machine.alias]
        machine.os_access.networking.setup_network(mac, ip)
    return {alias: ip.ip for alias, ip in host_ips.items()}
