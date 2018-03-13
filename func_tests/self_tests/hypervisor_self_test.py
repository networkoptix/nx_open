import logging
from pprint import pformat
from subprocess import check_call, check_output

import pytest

from test_utils.os_access import LocalAccess
from test_utils.virtual_box import VirtualBox

logger = logging.getLogger(__name__)


@pytest.fixture(scope='session')
def hypervisor():
    return VirtualBox(LocalAccess(), 'localhost')


name_prefix = 'func_tests-dummy-'
template_vm = name_prefix + 'template'
template_snapshot = 'test_template'


@pytest.fixture(scope='session')
def template():
    """Create VM without tested class."""
    for line in check_output(['VBoxManage', 'list', 'vms']).splitlines():
        name_quoted, _guid = line.rsplit(' ', 1)
        name = name_quoted[1:-1]  # Always quoted, not escaped.
        if name.startswith(name_prefix):
            check_call(['VBoxManage', 'unregistervm', name, '--delete'])
            logger.debug("Delete %s.", name)
        else:
            logger.debug("Skip %s.", name)
    logger.debug("Create template %s.", template_vm)
    check_call(['VBoxManage', 'createvm', '--name', template_vm, '--register'])
    check_call(['VBoxManage', 'modifyvm', template_vm, '--description', 'For testing purposes. Can be deleted.'])
    check_call(['VBoxManage', 'snapshot', template_vm, 'take', template_snapshot])


@pytest.mark.usefixtures('template')
def test_find(hypervisor):
    assert hypervisor.find(template_vm).name == template_vm


@pytest.mark.usefixtures('template')
def test_clone(hypervisor):
    clone_name = name_prefix + 'clone'
    clone = hypervisor.create(
        clone_name,
        {'vm': template_vm, 'snapshot': template_snapshot},
        [('ssh', 'tcp', 65022, 22), ('dns', 'udp', 65053, 53)])
    logger.debug("Clone:\n%s", pformat(clone))
    assert clone.name == clone_name
    assert not clone.is_running
    assert clone.ports['tcp', 22] == 65022
    assert clone.ports['udp', 53] == 65053
