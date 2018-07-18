import logging
from pprint import pformat
from subprocess import check_call

import pytest

from framework.vms.hypervisor import VMNotFound, VmHardware

_logger = logging.getLogger(__name__)

name_format = 'func_tests-temp-dummy-{}'


@pytest.fixture(scope='session')
def template(hypervisor):
    """Create Machine without tested class."""
    name = name_format.format('template')
    vm = hypervisor.create_dummy_vm(name, exists_ok=True)
    check_call(['VBoxManage', 'snapshot', name, 'take', 'template'])
    return vm


@pytest.fixture(scope='session')
def dummy(hypervisor):
    name = name_format.format('dummy')
    return hypervisor.create_dummy_vm(name, exists_ok=True)


@pytest.fixture()
def clone_name(hypervisor):
    name = name_format.format('clone')
    try:
        vm = hypervisor.find_vm(name)
    except VMNotFound:
        pass
    else:
        vm.destroy()
    return name


def test_find(hypervisor, dummy):
    assert isinstance(hypervisor.find_vm(dummy.name), VmHardware)


def test_clone(template, clone_name):
    clone = template.clone(clone_name)
    _logger.debug("Clone:\n%s", pformat(clone))
    assert clone.name == clone_name


def test_power(dummy):
    dummy.power_on()
    dummy.power_off()


def test_list(hypervisor):
    a_name = name_format.format('list-a')
    hypervisor.create_dummy_vm(a_name, exists_ok=True)
    assert a_name in hypervisor.list_vm_names()
    b_name = name_format.format('list-b')
    b = hypervisor.create_dummy_vm(b_name, exists_ok=True)
    assert a_name in hypervisor.list_vm_names()
    assert b_name in hypervisor.list_vm_names()
    b.destroy()
    assert a_name in hypervisor.list_vm_names()
    assert b_name not in hypervisor.list_vm_names()
