import logging
from pprint import pformat
from subprocess import call, check_call, check_output

import pytest

from test_utils.utils import wait_until
from test_utils.virtual_box import VMInfo

logger = logging.getLogger(__name__)

name_prefix = 'func_tests-dummy-'


@pytest.fixture(scope='session')
def template():
    """Create VM without tested class."""
    template_vm = name_prefix + 'template'
    template_snapshot = 'test_template'
    for line in check_output(['VBoxManage', 'list', 'vms']).splitlines():
        name_quoted, _guid = line.rsplit(' ', 1)
        name = name_quoted[1:-1]  # Always quoted, not escaped.
        if name.startswith(name_prefix):
            call(['VBoxManage', 'controlvm', name, 'poweroff'])
            check_call(['VBoxManage', 'unregistervm', name, '--delete'])
            logger.debug("Delete %s.", name)
        else:
            logger.debug("Skip %s.", name)
    logger.debug("Create template %s.", template_vm)
    check_call(['VBoxManage', 'createvm', '--name', template_vm, '--register'])
    check_call(['VBoxManage', 'modifyvm', template_vm, '--description', 'For testing purposes. Can be deleted.'])
    check_call(['VBoxManage', 'snapshot', template_vm, 'take', template_snapshot])
    return {'vm': template_vm, 'snapshot': template_snapshot}


@pytest.fixture(scope='session')
def dummy(hypervisor, template):
    dummy_name = name_prefix + 'dummy'
    return hypervisor.clone(dummy_name, template, [('ssh', 'tcp', 65022, 22)])


def test_find(hypervisor, template):
    assert isinstance(hypervisor.find(template['vm']), VMInfo)


def test_clone(hypervisor, template):
    clone_name = name_prefix + 'clone'
    clone = hypervisor.clone(clone_name, template, [('ssh', 'tcp', 65022, 22), ('dns', 'udp', 65053, 53)])
    logger.debug("Clone:\n%s", pformat(clone))
    assert clone.name == clone_name
    assert not clone.is_running
    assert clone.ports['tcp', 22] == (hypervisor.hostname, 65022)
    assert clone.ports['udp', 53] == (hypervisor.hostname, 65053)


def test_power(hypervisor, dummy):
    hypervisor.power_on(dummy.name)
    assert wait_until(lambda: hypervisor.find(dummy.name).is_running, name='until VM get started')
    hypervisor.power_off(dummy.name)
    assert wait_until(lambda: not hypervisor.find(dummy.name).is_running, name='until VM get shut down')
