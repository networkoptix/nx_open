import pytest
from netaddr import IPAddress, IPNetwork

from framework.networking import setup_flat_network, setup_networks
from framework.pool import ClosingPool
from framework.waiting import wait_for_true


@pytest.fixture()
def machines(vm_factory):
    with ClosingPool(vm_factory.allocated_vm, {}, 'linux') as machines:
        yield machines


def test_setup_basic(machines, hypervisor):
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

    nodes, ip_addresses = setup_networks(machines, hypervisor, structure, reachability)

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

    assert nodes['second'].networking.can_reach(ip_addresses['second'][IPNetwork('10.254.3.0/24')], timeout_sec=4)
    assert nodes['second'].networking.can_reach(ip_addresses['router-2'][IPNetwork('10.254.3.0/24')], timeout_sec=4)
    assert nodes['second'].networking.can_reach(ip_addresses['router-2'][IPNetwork('10.254.2.0/24')], timeout_sec=4)
    assert nodes['second'].networking.can_reach(ip_addresses['router-1'][IPNetwork('10.254.2.0/24')], timeout_sec=4)
    assert nodes['second'].networking.can_reach(ip_addresses['router-1'][IPNetwork('10.254.1.0/24')], timeout_sec=4)
    assert nodes['second'].networking.can_reach(ip_addresses['first'][IPNetwork('10.254.1.0/24')], timeout_sec=4)
    assert nodes['first'].networking.can_reach(ip_addresses['router-1'][IPNetwork('10.254.1.0/24')], timeout_sec=4)
    assert not nodes['first'].networking.can_reach(ip_addresses['router-1'][IPNetwork('10.254.2.0/24')], timeout_sec=2)
    assert not nodes['first'].networking.can_reach(ip_addresses['router-2'][IPNetwork('10.254.2.0/24')], timeout_sec=2)
    assert not nodes['first'].networking.can_reach(ip_addresses['second'][IPNetwork('10.254.3.0/24')], timeout_sec=2)


def test_linux_and_windows(linux_vm, windows_vm, hypervisor):
    ips = setup_flat_network((linux_vm, windows_vm), IPNetwork('10.254.0.0/28'), hypervisor)
    windows_vm_ip = ips[windows_vm.alias]
    wait_for_true(
        lambda: linux_vm.networking.can_reach(windows_vm_ip, timeout_sec=2),
        "{} can ping {} by {}".format(linux_vm, windows_vm, windows_vm_ip))
    linux_vm_ip = ips[linux_vm.alias]
    wait_for_true(
        lambda: windows_vm.networking.can_reach(linux_vm_ip, timeout_sec=2),
        "{} can ping {} by {}".format(windows_vm, linux_vm, linux_vm_ip))
