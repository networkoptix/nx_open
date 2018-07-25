import pytest
from contextlib2 import ExitStack
from netaddr import IPAddress, IPNetwork

from framework.networking import setup_flat_network, setup_networks
from framework.waiting import wait_for_true

pytest_plugins = ['fixtures.big_flat_networks']


@pytest.fixture()
def allocate_machine(vm_types):
    with ExitStack() as stack:
        def allocate(alias):
            return stack.enter_context(vm_types['linux'].allocated_vm(alias))

        yield allocate


def test_setup_basic(allocate_machine, hypervisor):
    structure = {
        '10.254.1.0/24': {
            'first': None,
            'router-1': {
                '10.254.2.0/24': {
                    'router-2': {
                        '10.254.3.0/24': {
                            'second': None}}}}}}
    reachability = {
        '10.254.1.0/24': {
            'first': {
                'second': None}}}

    nodes, ip_addresses = setup_networks(allocate_machine, hypervisor, structure, reachability)

    assert IPNetwork('10.254.1.0/24') in ip_addresses['first']
    assert IPNetwork('10.254.2.0/24') not in ip_addresses['first']
    assert IPNetwork('10.254.3.0/24') not in ip_addresses['first']
    assert IPNetwork('10.254.1.0/24') in ip_addresses['router-1']
    assert IPNetwork('10.254.2.0/24') in ip_addresses['router-1']
    assert IPNetwork('10.254.3.0/24') not in ip_addresses['router-1']
    assert IPNetwork('10.254.1.0/24') not in ip_addresses['router-2']
    assert IPNetwork('10.254.2.0/24') in ip_addresses['router-2']
    assert IPNetwork('10.254.3.0/24') in ip_addresses['router-2']
    assert IPNetwork('10.254.1.0/24') not in ip_addresses['second']
    assert IPNetwork('10.254.2.0/24') not in ip_addresses['second']
    assert IPNetwork('10.254.3.0/24') in ip_addresses['second']

    assert ip_addresses['router-1'][IPNetwork('10.254.2.0/24')] == IPAddress('10.254.2.1')
    assert ip_addresses['router-2'][IPNetwork('10.254.3.0/24')] == IPAddress('10.254.3.1')

    assert nodes['second'].os_access.networking.can_reach(ip_addresses['second'][IPNetwork('10.254.3.0/24')], timeout_sec=4)
    assert nodes['second'].os_access.networking.can_reach(ip_addresses['router-2'][IPNetwork('10.254.3.0/24')], timeout_sec=4)
    assert nodes['second'].os_access.networking.can_reach(ip_addresses['router-2'][IPNetwork('10.254.2.0/24')], timeout_sec=4)
    assert nodes['second'].os_access.networking.can_reach(ip_addresses['router-1'][IPNetwork('10.254.2.0/24')], timeout_sec=4)
    assert nodes['second'].os_access.networking.can_reach(ip_addresses['router-1'][IPNetwork('10.254.1.0/24')], timeout_sec=4)
    assert nodes['second'].os_access.networking.can_reach(ip_addresses['first'][IPNetwork('10.254.1.0/24')], timeout_sec=4)
    assert nodes['first'].os_access.networking.can_reach(ip_addresses['router-1'][IPNetwork('10.254.1.0/24')], timeout_sec=4)
    assert not nodes['first'].os_access.networking.can_reach(ip_addresses['router-1'][IPNetwork('10.254.2.0/24')], timeout_sec=2)
    assert not nodes['first'].os_access.networking.can_reach(ip_addresses['router-2'][IPNetwork('10.254.2.0/24')], timeout_sec=2)
    assert not nodes['first'].os_access.networking.can_reach(ip_addresses['second'][IPNetwork('10.254.3.0/24')], timeout_sec=2)


def test_two_vms(two_vm_types, vm_types, hypervisor):
    first_vm_type, second_vm_type = two_vm_types
    with vm_types[first_vm_type].allocated_vm('first-{}'.format(first_vm_type)) as first_vm:
        with vm_types[second_vm_type].allocated_vm('second-{}'.format(second_vm_type)) as second_vm:
            ips = setup_flat_network([first_vm, second_vm], IPNetwork('10.254.254.0/28'), hypervisor)
            second_vm_ip = ips[second_vm.alias]
            wait_for_true(
                lambda: first_vm.os_access.networking.can_reach(second_vm_ip),
                "{} can ping {} by {}".format(first_vm, second_vm, second_vm_ip))
            first_vm_ip = ips[first_vm.alias]
            wait_for_true(
                lambda: second_vm.os_access.networking.can_reach(first_vm_ip),
                "{} can ping {} by {}".format(second_vm, first_vm, first_vm_ip))


def test_big_flat_network(big_flat_network):
    machines, ips = big_flat_network
    random_machine = machines[0]
    for other_machine in machines[1:]:
        random_machine.os_access.networking.can_reach(ips[other_machine.alias])
