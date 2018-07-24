import logging
from multiprocessing.dummy import Pool as ThreadPool

import pytest
from contextlib2 import ExitStack
from netaddr import IPNetwork

from framework.networking import setup_flat_network

_logger = logging.getLogger(__name__)


@pytest.fixture(scope='session', params=[10], ids='{}_nodes'.format)
def big_flat_network(vm_factory, hypervisor, request):
    number_of_nodes = request.param
    aliases = ['machine-{:03d}'.format(machine_index) for machine_index in range(number_of_nodes)]
    pool = ThreadPool(processes=min(10, number_of_nodes))
    with ExitStack() as stack:
        def target(alias):
            try:
                return stack.enter_context(vm_factory.allocated_vm(alias, vm_type='linux'))
            except Exception as e:
                _logger.error("Exception raised. Original backtrace here.", exc_info=e)
                raise

        try:
            machines = pool.map_async(target, aliases).get()
        finally:
            pool.terminate()
            pool.join()
        ips = setup_flat_network(machines, IPNetwork('10.254.254.0/24'), hypervisor)
        yield machines, ips
