from collections import namedtuple

import pytest
from netaddr import IPAddress
from netaddr.ip import IPNetwork
from pathlib2 import Path

from framework.networking import setup_flat_network
from framework.os_access.local_access import LocalAccess
from framework.registry import Registry
from framework.serialize import load
from framework.vms.factory import VMFactory
from framework.vms.hypervisor.virtual_box import VirtualBox

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
def vm_registries(configuration, hypervisor):
    return {
        vm_type: Registry(
            hypervisor.host_os_access,
            hypervisor.host_os_access.Path(vm_type_configuration['registry_path']).expanduser(),
            vm_type_configuration['name_format'].format(vm_index='{index}'),  # Registry doesn't know about VMs.
            vm_type_configuration['limit'],
            )
        for vm_type, vm_type_configuration in configuration['vm_types'].items()}


@pytest.fixture(scope='session')
def vm_factory(request, configuration, hypervisor, vm_registries):
    factory = VMFactory(configuration['vm_types'], hypervisor, vm_registries)
    if request.config.getoption('--clean'):
        factory.cleanup()
    return factory


@pytest.fixture(scope='session')
def two_linux_vms(vm_factory, hypervisor):
    with vm_factory.allocated_vm('first') as first_vm, vm_factory.allocated_vm('second') as second_vm:
        setup_flat_network([first_vm, second_vm], IPNetwork('10.254.0.0/29'), hypervisor)
        yield first_vm, second_vm


@pytest.fixture(scope='session')
def linux_vm(vm_factory):
    with vm_factory.allocated_vm('single-linux-vm') as vm:
        yield vm


@pytest.fixture(scope='session')
def windows_vm(vm_factory):
    with vm_factory.allocated_vm('single-windows-vm', vm_type='windows') as vm:
        yield vm
