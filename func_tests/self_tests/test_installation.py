import logging

import pytest

from framework.installation.make_installation import installer_by_vm_type, make_installation
from framework.installation.mediaserver_factory import collect_artifacts_from_mediaserver, setup_clean_mediaserver
from framework.installation.unpack_installation import UnpackedMediaserverGroup
from framework.os_access.ssh_access import PhysicalSshAccess

_logger = logging.getLogger(__name__)


def test_install(one_vm, mediaserver_installers):
    installer = installer_by_vm_type(mediaserver_installers, one_vm.type)
    installation = make_installation(mediaserver_installers, one_vm.type, one_vm.os_access)
    installation.install(installer)
    assert installation.is_valid()



@pytest.fixture
def config(test_config):
    return test_config.with_defaults(
        SERVER_COUNT=5,
        SERVER_ROOT_DIR='/tmp/srv',
        OS_ACCESS=None,
        )


@pytest.fixture
def linux_multi_vm(vm_factory):
    with vm_factory.allocated_vm('single-multi', vm_type='linux_multi') as vm:
        yield vm

@pytest.fixture
def group_install_os_access(request, config):
    if config.OS_ACCESS:
        return PhysicalSshAccess(config.OS_ACCESS['address'], config.OS_ACCESS['username'], config.OS_ACCESS['key_path'])
    else:
        vm = request.getfixturevalue('linux_multi_vm')
        return vm.os_access


def test_group_install(artifact_factory, mediaserver_installers, ca, config, group_install_os_access):
    installer = installer_by_vm_type(mediaserver_installers, vm_type='linux')
    group = UnpackedMediaserverGroup(
        posix_access=group_install_os_access,
        installer=installer_by_vm_type(mediaserver_installers, 'linux'),
        root_dir=group_install_os_access.Path(config.SERVER_ROOT_DIR),
        base_port=7001,
        )
    installation_list = group.allocate_many(config.SERVER_COUNT)
    server_list = [setup_clean_mediaserver('server', installation, installer, ca)
                   for installation in installation_list]
    try:
        for server in server_list:
            server.start()
    finally:
        for server in server_list:
            collect_artifacts_from_mediaserver(server, artifact_factory)
