from collections import namedtuple
from itertools import combinations_with_replacement

import pytest
from netaddr import IPAddress
from netaddr.ip import IPNetwork
from pathlib2 import Path
from pylru import lrudecorator

from framework.networking import setup_flat_network
from framework.os_access.local_access import local_access
from framework.os_access.posix_shell import local_shell
from framework.registry import Registry
from framework.serialize import load
from framework.vms.factory import VMFactory
from framework.vms.hypervisor.virtual_box import VirtualBox

DEFAULT_VM_HOST_USER = 'root'
DEFAULT_VM_HOST_DIR = '/tmp/jenkins-test'


@lrudecorator(1)
def vm_types_configuration():
    path = Path(__file__).with_name('configuration.yaml')
    configuration = load(path.read_text())
    return configuration['vm_types']


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
        'Working directory at host with VirtualBox'))


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
def host_posix_shell():
    return local_shell


@pytest.fixture(scope='session')
def host_os_access():
    return local_access


@pytest.fixture(scope='session')
def hypervisor(host_os_access):
    return VirtualBox(host_os_access)


@pytest.fixture(scope='session')
def vm_registries(host_posix_shell, host_os_access):
    return {
        vm_type: Registry(
            host_posix_shell,
            host_os_access.Path(vm_type_configuration['registry_path']).expanduser(),
            vm_type_configuration['name_format'].format(vm_index='{index}'),  # Registry doesn't know about VMs.
            vm_type_configuration['limit'],
            )
        for vm_type, vm_type_configuration in vm_types_configuration().items()}


@pytest.fixture(scope='session')
def vm_factory(request, hypervisor, vm_registries):
    factory = VMFactory(vm_types_configuration(), hypervisor, vm_registries)
    if request.config.getoption('--clean'):
        factory.cleanup()
    return factory


@pytest.fixture(
    scope='session',
    params=vm_types_configuration().keys())
def one_vm_type(request):
    return request.param


@pytest.fixture(
    scope='session',
    params=combinations_with_replacement(vm_types_configuration().keys(), 2),
    ids='-'.join)
def two_vm_types(request):
    return request.param


@pytest.fixture(scope='session')
def one_vm(one_vm_type, vm_factory):
    with vm_factory.allocated_vm('single-{}'.format(one_vm_type), vm_type=one_vm_type) as vm:
        yield vm


@pytest.fixture(scope='session')
def two_vms(two_vm_types, hypervisor, vm_factory):
    first_vm_type, second_vm_type = two_vm_types
    with vm_factory.allocated_vm('first-{}'.format(first_vm_type), vm_type=first_vm_type) as first_vm:
        with vm_factory.allocated_vm('second-{}'.format(second_vm_type), vm_type=second_vm_type) as second_vm:
            setup_flat_network([first_vm, second_vm], IPNetwork('10.254.254.0/28'), hypervisor)
            yield first_vm, second_vm
