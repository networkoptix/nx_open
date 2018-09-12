import logging

import pytest
from contextlib2 import ExitStack
from netaddr import IPAddress, IPNetwork

from framework.networking import setup_flat_network, setup_networks
from framework.waiting import wait_for_truthy

pytest_plugins = ['fixtures.big_flat_networks']

_logger = logging.getLogger(__name__)


@pytest.fixture()
def allocate_machine(vm_types):
    with ExitStack() as stack:
        def allocate(alias):
            return stack.enter_context(vm_types['linux'].vm_ready(alias))

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


def test_two_vms(two_vms, hypervisor):
    first_vm, second_vm = two_vms
    for first_vm_ip in first_vm.os_access.networking.list_ips():
        _logger.info('Checking first vm ip address: %r', first_vm_ip)
        wait_for_truthy(
            lambda: second_vm.os_access.networking.can_reach(first_vm_ip),
            "{} can reach {} by {}".format(second_vm, first_vm, first_vm_ip))
    for second_vm_ip in second_vm.os_access.networking.list_ips():
        _logger.info('Checking second vm ip address: %r', second_vm_ip)
        wait_for_truthy(
            lambda: first_vm.os_access.networking.can_reach(second_vm_ip),
            "{} can reach {} by {}".format(first_vm, second_vm, second_vm_ip))


def test_big_flat_network(big_flat_network):
    machines, ips = big_flat_network
    random_machine = machines[0]
    for other_machine in machines[1:]:
        random_machine.os_access.networking.can_reach(ips[other_machine.alias])
