import pytest
from netaddr import IPNetwork

from framework.networking import setup_flat_network
from framework.pool import ClosingPool


def make_big_flat_network(vm_factory, hypervisor, number_of_nodes):
    aliases = ['machine-{:03d}'.format(machine_index) for machine_index in range(number_of_nodes)]
    with ClosingPool(vm_factory.allocated_vm, {}, 'linux') as pool:
        machines = pool.get_many(aliases, parallel_jobs=10)
        ips = setup_flat_network(machines, IPNetwork('10.254.254.0/24'), hypervisor)
    return machines, ips


@pytest.fixture(scope='session', params=[10], ids='{}_nodes'.format)
def big_flat_network(vm_factory, hypervisor, request):
    return make_big_flat_network(vm_factory, hypervisor, request.param)
