from itertools import combinations_with_replacement

import pytest
from netaddr.ip import IPNetwork
from pathlib2 import Path
from pylru import lrudecorator

from framework.networking import setup_flat_network
from framework.os_access.local_access import local_access
from framework.serialize import load
from framework.vms.hypervisor.virtual_box import VirtualBox
from framework.vms.vm_type import VMType


@lrudecorator(1)
def vm_types_configuration():
    # It's a function, not a fixture, because used when parameters are generated.
    path = Path(__file__).with_name('configuration.yaml')
    configuration = load(path.read_text())
    return configuration['vm_types']


@pytest.fixture(scope='session')
def host_os_access():
    return local_access


@pytest.fixture(scope='session')
def hypervisor(host_os_access):
    return VirtualBox(host_os_access)


@pytest.fixture(scope='session')
def vm_types(request, hypervisor):
    vm_types = {
        vm_type_name: VMType(
            hypervisor,
            vm_type_conf['os_family'],
            vm_type_conf['power_on_timeout_sec'],
            **vm_type_conf['vm'])
        for vm_type_name, vm_type_conf in vm_types_configuration().items()
        }
    if request.config.getoption('--clean'):
        for vm_type in vm_types.values():
            vm_type.cleanup()
    return vm_types


def vm_type_list():
    return [name for name, conf in vm_types_configuration().items()
                if not conf.get('is_custom', False)]

@pytest.fixture(
    scope='session',
    params=vm_type_list())
def one_vm_type(request):
    return request.param


@pytest.fixture(
    scope='session',
    params=combinations_with_replacement(vm_type_list(), 2),
    ids='-'.join)
def two_vm_types(request):
    return request.param


@pytest.fixture(scope='session')
def linux_vm(vm_types):
    with vm_types['linux'].allocated_vm('single-linux') as vm:
        yield vm


@pytest.fixture(scope='session')
def windows_vm(vm_types):
    with vm_types['windows'].allocated_vm('single-windows') as vm:
        yield vm


@pytest.fixture(scope='session')
def one_vm(request, one_vm_type):
    # TODO: If new VM type is added, create separate fixture for it or use factory here.
    return request.getfixturevalue(one_vm_type + '_vm')


@pytest.fixture(scope='session')
def two_vms(two_vm_types, hypervisor, vm_types):
    first_vm_type, second_vm_type = two_vm_types
    with vm_types[first_vm_type].allocated_vm('first-{}'.format(first_vm_type)) as first_vm:
        with vm_types[second_vm_type].allocated_vm('second-{}'.format(second_vm_type)) as second_vm:
            setup_flat_network([first_vm, second_vm], IPNetwork('10.254.254.0/28'), hypervisor)
            yield first_vm, second_vm
