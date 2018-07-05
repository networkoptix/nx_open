import logging
from pprint import pformat
from subprocess import call, check_call

import pytest

from framework.vms.hypervisor import VMInfo, VMNotFound, TemplateVMNotFound
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)

name_format = 'func_tests-temp-dummy-{}'


@pytest.fixture(scope='session')
def template(hypervisor):
    """Create Machine without tested class."""
    vm = name_format.format('template')
    try:
        hypervisor.destroy(vm)
    except VMNotFound:
        pass
    hypervisor.create_dummy(vm)
    check_call(['VBoxManage', 'snapshot', vm, 'take', 'template'])
    return vm


@pytest.fixture(scope='session')
def dummy(hypervisor):
    name = name_format.format('dummy')
    try:
        hypervisor.destroy(name)
    except VMNotFound:
        pass
    hypervisor.create_dummy(name)
    return name


@pytest.fixture()
def clone_name(hypervisor):
    name = name_format.format('clone')
    try:
        hypervisor.destroy(name)
    except VMNotFound:
        pass
    return name


def test_find(hypervisor, dummy):
    assert isinstance(hypervisor.find(dummy), Vm)


def test_clone(hypervisor, template, clone_name):
    hypervisor.clone(template, clone_name)
    clone = hypervisor.find(clone_name)
    _logger.debug("Clone:\n%s", pformat(clone))
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
        hypervisor.create_dummy(name)
    assert set(names) <= set(hypervisor.list_vm_names())
    for name in names:
        hypervisor.destroy(name)
    assert not set(names) & set(hypervisor.list_vm_names())


def test_template_not_found_error(hypervisor):
    with pytest.raises(TemplateVMNotFound) as excinfo:
        hypervisor.clone('missing-vm-source', 'unused-vm-target')
    assert excinfo.value.message == "Template VM is missing: 'missing-vm-source'"
