from contextlib import closing

import pytest

from test_utils.merging import setup_local_system, merge_systems
from test_utils.pool import Pool
from test_utils.server_factory import ServerFactory


@pytest.fixture()
def linux_server_factory(mediaserver_deb, ca, artifact_factory, vm_pools):
    return ServerFactory(artifact_factory, vm_pools['linux'], mediaserver_deb, ca)


@pytest.fixture()
def linux_servers_pool(linux_server_factory):
    with closing(Pool(linux_server_factory)) as pool:
        yield pool


@pytest.fixture()
def two_linux_servers(artifact_factory, two_linux_vms, mediaserver_deb, ca, cloud_host):
    factory = ServerFactory(artifact_factory, two_linux_vms, mediaserver_deb, ca)
    with closing(Pool(factory)) as pool:
        servers = pool.get('first'), pool.get('second')
        for server in servers:
            server.installation.patch_binary_set_cloud_host(cloud_host)
        yield servers


@pytest.fixture()
def two_running_linux_servers(two_linux_servers):
    for server in two_linux_servers:
        server.start()
        setup_local_system(server, {})
    return two_linux_servers


@pytest.fixture()
def two_merged_linux_servers(two_running_linux_servers):
    merge_systems(*two_running_linux_servers)
    return two_running_linux_servers


@pytest.fixture()
def linux_server(linux_vm, artifact_factory, mediaserver_deb, ca, cloud_host):
    factory = ServerFactory(artifact_factory, linux_vm, mediaserver_deb, ca)
    with closing(Pool(factory)) as pool:
        server = pool.get('single')
        server.installation.patch_binary_set_cloud_host(cloud_host)
        yield server


@pytest.fixture()
def running_linux_server(linux_server):
    linux_server.start()
    setup_local_system(linux_server, {})
    return linux_server
