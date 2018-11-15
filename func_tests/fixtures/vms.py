import pytest
from netaddr.ip import IPNetwork

from defaults import defaults
from framework.os_access.posix_access import local_access
from framework.vms.hypervisor.virtual_box import VirtualBox
from framework.vms.networks import setup_flat_network
from framework.vms.vm_type import vm_types_from_config


def pytest_addoption(parser):
    parser.addoption(
        '--template-url-prefix', '-T', default=defaults.get('template_url_prefix'),
        help=(
            "Prefix for template_file parameter in configuration file. "
            "Supports: http://, https://, smb://, file://. "))


@pytest.fixture(scope='session')
def host_os_access():
    return local_access


@pytest.fixture(scope='session')
def runner_address(hypervisor):
    return hypervisor.runner_address


@pytest.fixture(scope='session')
def persistent_dir(slot, host_os_access):
    dir = host_os_access.path_cls.home() / '.func_tests' / 'slot_{}'.format(slot)
    dir.mkdir(exist_ok=True, parents=True)
    return dir


@pytest.fixture(scope='session')
def hypervisor(host_os_access):
    return VirtualBox(host_os_access=host_os_access)


@pytest.fixture(scope='session')
def vm_types(request, slot, hypervisor, persistent_dir):
    template_url_prefix = request.config.getoption('template_url_prefix')
    vm_types = vm_types_from_config(
        downloads_dir=persistent_dir,
        template_url_prefix=template_url_prefix,
        hypervisor=hypervisor,
        slot=slot)
    if request.config.getoption('--clean'):
        for vm_type in vm_types.values():
            vm_type.cleanup()
    return vm_types


@pytest.fixture(
    scope='session',
    params=['linux', 'windows'])
def one_vm_type(request):
    return request.param


@pytest.fixture(
    scope='session',
    params=[
        ('linux', 'linux'),
        ('linux', 'windows'),
        ('windows', 'windows'),
        ],
    ids='-'.join)
def two_vm_types(request):
    return request.param


@pytest.fixture
def linux_vm(vm_types):
    with vm_types['linux'].vm_ready('single-linux') as vm:
        yield vm


@pytest.fixture
def windows_vm(vm_types):
    with vm_types['windows'].vm_ready('single-windows') as vm:
        yield vm


@pytest.fixture
def one_vm(request, one_vm_type):
    # TODO: If new VM type is added, create separate fixture for it or use factory here.
    return request.getfixturevalue(one_vm_type + '_vm')


@pytest.fixture
def two_vms(two_vm_types, hypervisor, vm_types):
    first_vm_type, second_vm_type = two_vm_types
    with vm_types[first_vm_type].vm_ready('first-{}'.format(first_vm_type)) as first_vm:
        with vm_types[second_vm_type].vm_ready('second-{}'.format(second_vm_type)) as second_vm:
            setup_flat_network([first_vm, second_vm], IPNetwork('10.254.254.0/28'), hypervisor)
            yield first_vm, second_vm
