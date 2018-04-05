from collections import namedtuple
from contextlib import closing

import pytest
from netaddr import IPAddress
from pathlib2 import Path

from framework.merging import setup_system
from framework.networking import setup_networks
from framework.os_access.local import LocalAccess
from framework.pool import Pool
from framework.registry import Registry
from framework.serialize import load
from framework.vms.factory import VMFactory
from framework.vms.hypervisor.virtual_box import VirtualBox
from network_layouts import get_layout

DEFAULT_VM_HOST_USER = 'root'
DEFAULT_VM_HOST_DIR = '/tmp/jenkins-test'


def pytest_addoption(parser):
    parser.addoption('--vm-port-base', help=(
        'Deprecated. Left for backward compatibility. Ignored.'))
    parser.addoption('--vm-name-prefix', help=(
        'Deprecated. Left for backward compatibility. Ignored.'))
    parser.addoption('--vm-address', type=IPAddress, help=(
        'IP address virtual machines bind to. '
        'Test camera discovery will answer only to this address if this option is specified.'))
    parser.addoption('--vm-host', help=(
        'hostname or IP address for host with VirtualBox, '
        'used to start virtual machines (by default it is local host)'))
    parser.addoption('--vm-host-user', default=DEFAULT_VM_HOST_USER, help=(
        'User to use for ssh to login to VirtualBox host'))
    parser.addoption('--vm-host-key', help=(
        'Identity file to use for ssh to login to VirtualBox host'))
    parser.addoption('--vm-host-dir', default=DEFAULT_VM_HOST_DIR, help=(
        'Working directory at host with VirtualBox, used to store vagrant files'))


@pytest.fixture(scope='session')
def vm_address(request):
    return request.config.getoption('--vm-address')


VMHost = namedtuple('VMHost', ['hostname', 'username', 'private_key', 'work_dir', 'vm_port_base', 'vm_name_prefix'])


@pytest.fixture(scope='session')
def vm_host(request):
    return VMHost(
        hostname=request.config.getoption('--vm-host'),
        work_dir=request.config.getoption('--vm-host-dir'),
        vm_name_prefix=request.config.getoption('--vm-name-prefix'),
        vm_port_base=request.config.getoption('--vm-port-base'),
        username=request.config.getoption('--vm-host-user'),
        private_key=request.config.getoption('--vm-host-key'))


@pytest.fixture(scope='session')
def configuration():
    return load(Path(__file__).with_name('configuration.yaml').read_text())


@pytest.fixture(scope='session')
def host_os_access():
    return LocalAccess()


@pytest.fixture(scope='session')
def hypervisor(configuration, host_os_access):
    return VirtualBox(host_os_access, configuration['vm_host']['address'])


@pytest.fixture(scope='session')
def vm_registries(configuration, host_os_access):
    return {
        vm_type: Registry(host_os_access, vm_configuration['registry_path'])
        for vm_type, vm_configuration in configuration['vm_types'].items()
        }


@pytest.fixture(scope='session')
def vm_factories(request, configuration, hypervisor, vm_registries):
    factories = {
        vm_type: VMFactory(vm_configuration, hypervisor, vm_registries[vm_type])
        for vm_type, vm_configuration in configuration['vm_types'].items()
        }
    if request.config.getoption('--clean'):
        for vm_factory in factories.values():
            vm_factory.cleanup()
    return factories


@pytest.fixture()
def vm_pools(vm_factories):
    pools = {
        vm_type: Pool(vm_factory)
        for vm_type, vm_factory in vm_factories.items()
        }
    yield pools
    for pool in pools.values():
        pool.close()


@pytest.fixture()
def linux_vm_pool(vm_factories):
    with closing(Pool(vm_factories['linux'])) as pool:
        yield pool


@pytest.fixture()
def network(vm_pools, hypervisor, linux_mediaservers_pool, layout_file):
    layout = get_layout(layout_file)
    vms, _ = setup_networks(vm_pools, hypervisor, layout.networks, {})
    servers = setup_system(linux_mediaservers_pool, layout.mergers)
    return vms, servers


@pytest.fixture(scope='session')
def two_linux_vms(vm_factories, hypervisor):
    structure = {'10.254.0.0/29': {'first': None, 'second': None}}
    reachability = {'10.254.0.0/29': {'first': {'second': None}, 'second': {'first': None}}}
    with closing(Pool(vm_factories['linux'])) as pool:
        vms, _ = setup_networks({'linux': pool}, hypervisor, structure, reachability)
        yield vms


@pytest.fixture(scope='session')
def linux_vm(vm_factories):
    with closing(Pool(vm_factories['linux'])) as pool:
        yield pool.get('single')
