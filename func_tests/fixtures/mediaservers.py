from contextlib import closing

import pytest

from framework.merging import setup_local_system, merge_systems
from framework.pool import Pool
from framework.mediaserver_factory import MediaserverFactory


@pytest.fixture()
def linux_mediaserver_factory(mediaserver_deb, ca, artifact_factory, vm_pools):
    return MediaserverFactory(artifact_factory, vm_pools['linux'], mediaserver_deb, ca)


@pytest.fixture()
def linux_mediaservers_pool(linux_mediaserver_factory):
    with closing(Pool(linux_mediaserver_factory)) as pool:
        yield pool


@pytest.fixture()
def two_linux_mediaservers(artifact_factory, two_linux_vms, mediaserver_deb, ca, cloud_host):
    factory = MediaserverFactory(artifact_factory, two_linux_vms, mediaserver_deb, ca)
    with closing(Pool(factory)) as pool:
        servers = pool.get('first'), pool.get('second')
        for server in servers:
            server.installation.patch_binary_set_cloud_host(cloud_host)
        yield servers


@pytest.fixture()
def two_running_linux_mediaservers(two_linux_mediaservers):
    for server in two_linux_mediaservers:
        server.start()
        setup_local_system(server, {})
    return two_linux_mediaservers


@pytest.fixture()
def two_merged_linux_mediaservers(two_running_linux_mediaservers):
    merge_systems(*two_running_linux_mediaservers)
    return two_running_linux_mediaservers


@pytest.fixture()
def linux_mediaserver(linux_vm, artifact_factory, mediaserver_deb, ca, cloud_host):
    factory = MediaserverFactory(artifact_factory, linux_vm, mediaserver_deb, ca)
    with closing(Pool(factory)) as pool:
        server = pool.get('single')
        server.installation.patch_binary_set_cloud_host(cloud_host)
        yield server


@pytest.fixture()
def running_linux_mediaserver(linux_mediaserver):
    linux_mediaserver.start()
    setup_local_system(linux_mediaserver, {})
    return linux_mediaserver
