from netaddr import IPAddress, IPNetwork
from pathlib2 import Path

from test_utils.networking import setup_networks

SAMPLES_DIR = Path(__file__).parent / 'test_utils' / 'networking'


def test_setup_basic(vm_pools, hypervisor):
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
    nodes, ip_addresses = setup_networks(vm_pools, hypervisor, structure, reachability)

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
