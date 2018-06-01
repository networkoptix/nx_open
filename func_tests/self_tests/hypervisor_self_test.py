import logging
from pprint import pformat
from subprocess import call, check_call

import pytest

from framework.vms.hypervisor import VMInfo
from framework.waiting import wait_for_true

logger = logging.getLogger(__name__)

name_format = 'func_tests-temp-dummy-{}'


def _remove_vm(name):
    logger.debug("Delete %s if exists.", name)
    call(['VBoxManage', 'controlvm', name, 'poweroff'])
    call(['VBoxManage', 'unregistervm', name, '--delete'])


def _create_vm(name):
    _remove_vm(name)
    logger.debug("Create template %s.", name)
    check_call(['VBoxManage', 'createvm', '--name', name, '--register'])
    check_call(['VBoxManage', 'modifyvm', name, '--description', 'For testing purposes. Can be deleted.'])


@pytest.fixture(scope='session')
def template():
    """Create Machine without tested class."""
    vm = name_format.format('template')
    _create_vm(vm)
    vm_snapshot = 'test template'
    check_call(['VBoxManage', 'snapshot', vm, 'take', vm_snapshot])
    return vm, vm_snapshot


@pytest.fixture(scope='session')
def clone_configuration(template):
    template_vm, template_vm_snapshot = template
    return {
        'template_vm': template_vm,
        'template_vm_snapshot': template_vm_snapshot,
        'mac_address_format': '0A-00-00-FF-{vm_index:02X}-{nic_index:02X}',
        'port_forwarding': {
            'host_ports_base': 65000,
            'host_ports_per_vm': 5,
            'forwarded_ports': {
                'ssh': {'protocol': 'tcp', 'guest_port': 22, 'host_port_offset': 2},
                'dns': {'protocol': 'udp', 'guest_port': 53, 'host_port_offset': 3},
                }
            }
        }


@pytest.fixture(scope='session')
def dummy():
    name = name_format.format('dummy')
    _create_vm(name)
    return name


@pytest.fixture()
def clone_name():
    name = name_format.format('clone')
    call(['VBoxManage', 'controlvm', name, 'poweroff'])
    call(['VBoxManage', 'unregistervm', name, '--delete'])
    return name


def test_find(hypervisor, dummy):
    assert isinstance(hypervisor.find(dummy), VMInfo)


def test_clone(hypervisor, clone_name, clone_configuration):
    clone = hypervisor.clone(clone_name, 1, clone_configuration)
    logger.debug("Clone:\n%s", pformat(clone))
    assert clone.name == clone_name


def test_power(hypervisor, dummy):
    hypervisor.power_on(dummy)
    wait_for_true(lambda: hypervisor.find(dummy).is_running, 'Machine {} is running'.format(dummy))
    hypervisor.power_off(dummy)
    wait_for_true(lambda: not hypervisor.find(dummy).is_running, 'Machine {} is not running'.format(dummy))


def test_list(hypervisor):
    names = [
        name_format.format('test_list-{}'.format(index))
        for index in range(3)]
    for name in names:
        _create_vm(name)
    assert set(names) <= set(hypervisor.list_vm_names())
    for name in names:
        _remove_vm(name)
    assert not set(names) & set(hypervisor.list_vm_names())
