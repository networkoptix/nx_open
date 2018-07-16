import logging
from collections import namedtuple

import pytest
from pathlib2 import Path

from framework.utils import flatten_list, bool_to_str
from framework.os_access.ssh_access import PhysicalSshAccess
from framework.installation.make_installation import installer_by_vm_type, make_installation
from framework.installation.mediaserver_factory import collect_artifacts_from_mediaserver, setup_clean_mediaserver
from framework.installation.unpack_installation import UnpackedMediaserverGroup
from framework.installation.mediaserver import Mediaserver
from framework.installation.mediaserver_factory import (
    examine_mediaserver,
    collect_artifacts_from_mediaserver,
    )
from framework.installation.lightweight_mediaserver import LwMultiServer
from framework.merging import REMOTE_ADDRESS_ANY, setup_local_system, merge_systems

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
        SERVER_PORT_BASE=7001,
        LWS_SERVER_COUNT=5,
        LWS_PORT_BASE=8001,
        MERGE_TIMEOUT_SEC=5*60,
        HOST_LIST=None,
        )


@pytest.fixture
def linux_multi_vm(vm_factory):
    with vm_factory.allocated_vm('single-multi', vm_type='linux_multi') as vm:
        yield vm


Host = namedtuple('Host', 'name os_access')


def host_from_config(host_config):
    ssh_key = Path(host_config['key_path']).read_text()
    return Host(
        name=host_config['name'],
        os_access=PhysicalSshAccess(
            host_config['address'],
            host_config['username'],
            ssh_key,
            ),
        )

@pytest.fixture
def host_list(request, config):
    if config.HOST_LIST:
        return [host_from_config(host_config) for host_config in config.HOST_LIST]
    else:
        vm = request.getfixturevalue('linux_multi_vm')
        return [Host('vm', vm.os_access)]


def make_group_list(ca, installer, server_root_dir, server_port_base, host_list):
    return [UnpackedMediaserverGroup(
        posix_access=host.os_access,
        installer=installer,
        root_dir=host.os_access.Path(server_root_dir),
        base_port=server_port_base,
        )
        for host in host_list]

def make_host_server_list(ca, installer, host_name, group, count):
    installation_list = group.allocate_many(count)
    return [
        make_server(ca, installation, installer, host_name, index)
        for index, installation in enumerate(installation_list)]


system_settings = dict(
    autoDiscoveryEnabled=bool_to_str(False),
    synchronizeTimeWithInternet=bool_to_str(False),
    )

def make_server(ca, installation, installer, host_name, index):
    installation.install(installer)
    installation.cleanup(ca.generate_key_and_cert())
    server = Mediaserver('{}-{:03d}'.format(host_name, index), installation, port=installation.server_port)
    server.start()
    setup_local_system(server, system_settings)
    return server


def test_group_install(artifact_factory, mediaserver_installers, ca, config, host_list):
    installer = installer_by_vm_type(mediaserver_installers, vm_type='linux')
    group_list = make_group_list(ca, installer, config.SERVER_ROOT_DIR, config.SERVER_PORT_BASE, host_list)
    count_per_host = config.SERVER_COUNT // len(host_list)
    server_list = flatten_list(
        [make_host_server_list(ca, installer, host.name, group, count_per_host)
             for host, group in zip(host_list, group_list)])
    try:
        for server in server_list[1:]:
            merge_systems(server_list[0], server, remote_address=REMOTE_ADDRESS_ANY)
    finally:
        for server in server_list:
            examine_mediaserver(server)


def test_lightweight_install(
        artifact_factory,
        mediaserver_installers,
        lightweight_mediaserver_installer,
        ca,
        config,
        host_list,
        ):
    installer = installer_by_vm_type(mediaserver_installers, vm_type='linux')
    group_list = make_group_list(ca, installer, config.SERVER_ROOT_DIR, config.SERVER_PORT_BASE, host_list)
    group = group_list[0]  # use only first host for lws test
    installation = group.allocate()
    server = make_server(ca, installation, installer, 'standalone', index=0)
    group.lws.install(installer, lightweight_mediaserver_installer)
    group.lws.write_control_script(
        server_port_base=config.LWS_PORT_BASE,
        server_count=config.LWS_SERVER_COUNT,
        CAMERAS_PER_SERVER=20,
        )
    lws = LwMultiServer(group.lws)
    lws.start()
    lws.wait_until_synced(config.MERGE_TIMEOUT_SEC)
    # accessible_ip_net: any ip will work
    merge_systems(server, lws[0], take_remote_settings=True, remote_address=lws.address)
