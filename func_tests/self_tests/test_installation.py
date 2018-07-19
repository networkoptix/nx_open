import logging

import pytest

from framework.installation.make_installation import installer_by_vm_type, make_installation
from framework.merging import REMOTE_ADDRESS_ANY, merge_systems
from framework.utils import bool_to_str

pytest_plugins = ['fixtures.unpacked_mediaservers']

_logger = logging.getLogger(__name__)


def test_install(one_vm, mediaserver_installers):
    installer = installer_by_vm_type(mediaserver_installers, one_vm.type)
    installation = make_installation(mediaserver_installers, one_vm.type, one_vm.os_access)
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
def linux_multi_vm(vm_factory):
    with vm_factory.allocated_vm('single-multi', vm_type='linux_multi') as vm:
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
    with groups.allocated_many_servers(config.SERVER_COUNT, system_settings) as server_list:
        for server in server_list[1:]:
            merge_systems(server_list[0], server, remote_address=REMOTE_ADDRESS_ANY)


def test_lightweight_install(config, groups):
    with groups.allocated_one_server('standalone', system_settings) as server:
        with groups.allocated_lws(
                server_count=config.LWS_SERVER_COUNT,
                merge_timeout_sec=config.MERGE_TIMEOUT_SEC,
                CAMERAS_PER_SERVER=20,
                ) as lws:
            merge_systems(server, lws[0], take_remote_settings=True, remote_address=lws.address)
