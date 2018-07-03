import logging

import pytest

from framework.installation.make_installation import installer_by_vm_type, make_installation
from framework.installation.unpack_installation import UnpackedMediaserverGroup
from framework.installation.mediaserver_factory import setup_clean_mediaserver, collect_artifacts_from_mediaserver

_logger = logging.getLogger(__name__)


def test_install(one_vm, mediaserver_installers):
    installer = installer_by_vm_type(mediaserver_installers, one_vm.type)
    installation = make_installation(mediaserver_installers, one_vm.type, one_vm.os_access)
    installation.install(installer)
    assert installation.is_valid()


@pytest.fixture
def linux_multi_vm(vm_factory):
    with vm_factory.allocated_vm('single-multi', vm_type='linux_multi') as vm:
        yield vm

def test_group_install(artifact_factory, mediaserver_installers, ca, linux_multi_vm):
    installer = installer_by_vm_type(mediaserver_installers, vm_type='linux')
    group = UnpackedMediaserverGroup(
        linux_multi_vm.os_access, mediaserver_installers['linux64'], linux_multi_vm.os_access.Path('/tmp/srv'))
    installation_list = group.make_installations(7001, 5)
    server_list = [setup_clean_mediaserver('server', installation, installer, ca)
                   for installation in installation_list]
    try:
        for server in server_list:
            server.start()
    finally:
        for server in server_list:
            collect_artifacts_from_mediaserver(server, artifact_factory)
