import logging

import pytest
from netaddr import IPNetwork
from pathlib2 import PurePath

from framework.installation.installer import Installer
from framework.installation.make_installation import make_installation
from framework.merging import merge_systems
from framework.utils import bool_to_str

pytest_plugins = ['fixtures.unpacked_mediaservers']

_logger = logging.getLogger(__name__)


def test_install(one_vm, mediaserver_installer_set):
    installation = make_installation(one_vm.os_access, mediaserver_installer_set.customization)
    installer = mediaserver_installer_set.find_by_filter(installation.can_install)
    installation.install(installer)
    assert installation.is_valid()


# use --tests-config-file= pytest option to run on physical servers with specific server count and timeout
@pytest.fixture
def config(test_config):
    return test_config.with_defaults(
        SERVER_COUNT=5,
        LWS_SERVER_COUNT=5,
        HOST_LIST=None,
        MERGE_TIMEOUT_SEC=5*60,
        )

@pytest.fixture
def linux_multi_vm(vm_types):
    with vm_types['linux_multi'].vm_ready('single-multi') as vm:
        yield vm

@pytest.fixture
def groups(request, unpacked_mediaserver_factory, config):
    if config.HOST_LIST:
        return unpacked_mediaserver_factory.from_host_config_list(config.HOST_LIST)
    else:
        vm = request.getfixturevalue('linux_multi_vm')
        return unpacked_mediaserver_factory.from_vm(
            vm,
            dir='/test',
            server_port_base=7001,
            lws_port_base=8001,
            )


system_settings = dict(
    autoDiscoveryEnabled=bool_to_str(False),
    synchronizeTimeWithInternet=bool_to_str(False),
    )


def test_group_install(config, groups):
    with groups.many_allocated_servers(config.SERVER_COUNT, system_settings) as server_list:
        for server in server_list[1:]:
            merge_systems(server_list[0], server, accessible_ip_net=IPNetwork('0.0.0.0/0'))


def test_lightweight_install(config, groups):
    with groups.one_allocated_server('standalone', system_settings) as server:
        with groups.allocated_lws(
                server_count=config.LWS_SERVER_COUNT,
                merge_timeout_sec=config.MERGE_TIMEOUT_SEC,
                CAMERAS_PER_SERVER=20,
                ) as lws:
            server.api.merge(lws[0].api, lws.address, lws[0].port, take_remote_settings=True)


def test_unpack_core_dump(artifacts_dir, groups):
    with groups.one_allocated_server('standalone', system_settings) as server:
        status = server.service.status()
        assert status.is_running
        server.os_access.make_core_dump(status.pid)
        assert len(server.installation.list_core_dumps()) == 1
        server_name = server.name
    # expecting core file itself and it's traceback
    assert len(list(artifacts_dir.joinpath(server_name).glob('core.*'))) == 2


def test_lws_core_dump(artifacts_dir, config, groups):
    with groups.allocated_lws(
            server_count=config.LWS_SERVER_COUNT,
            merge_timeout_sec=config.MERGE_TIMEOUT_SEC,
            ) as lws:
        status = lws.service.status()
        assert status.is_running
        lws.os_access.make_core_dump(status.pid)
        assert len(lws.installation.list_core_dumps()) == 1
        server_name = lws.name
    # expecting core file itself and it's traceback
    assert len(list(artifacts_dir.joinpath(server_name).glob('core.*'))) == 2


@pytest.mark.parametrize(
    'path',
    [
        PurePath('/tmp/nxwitness-server-3.2.0.20805-linux64.deb'),
        PurePath('/tmp/nxwitness-server-4.0.0.2049-linux64-beta-test.deb'),
        PurePath('/tmp/nxwitness-server-4.0.0.2049-win64-beta-test.exe'),
        PurePath('/tmp/nxwitness-server-3.2.0.2032-mac-beta-test.dmg'),
        PurePath('/tmp/nxwitness-server-3.2.0.2032-bpi-beta-test.tar.gz'),
        PurePath('/tmp/nxwitness-server-3.2.0.2032-win64-beta-test.exe'),
        PurePath('/tmp/nxwitness-server-3.2.0.2032-win64-beta-test.zip'),
        PurePath('/tmp/wave-server-3.2.0.40235-bananapi-beta-test.zip'),
        PurePath('/tmp/dwspectrum-server-3.2.0.40235-edge1-beta-test.zip'),
        PurePath('/tmp/wave-server-3.2.0.40238-win86-beta-test.msi'),
        ],
    ids=lambda path: path.name
    )
def test_installer_name_parse(path):
    Installer(path)
