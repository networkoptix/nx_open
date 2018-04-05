import pytest

from framework.mediaserver_factory import MediaserverFactory
from framework.merging import merge_systems, setup_local_system


@pytest.fixture()
def mediaserver_factory(artifact_factory, mediaserver_deb, ca, cloud_host):
    return MediaserverFactory(artifact_factory, mediaserver_deb, ca, cloud_host)


@pytest.fixture()
def two_linux_mediaservers(mediaserver_factory, two_linux_vms):
    first_vm, second_vm = two_linux_vms
    with mediaserver_factory.allocated_mediaserver('first', first_vm) as first_mediaserver:
        with mediaserver_factory.allocated_mediaserver('second', second_vm) as second_mediaserver:
            yield first_mediaserver, second_mediaserver


@pytest.fixture()
def two_running_linux_mediaservers(two_linux_mediaservers):
    for mediaserver in two_linux_mediaservers:
        mediaserver.start()
        setup_local_system(mediaserver, {})
    return two_linux_mediaservers


@pytest.fixture()
def two_merged_linux_mediaservers(two_running_linux_mediaservers):
    merge_systems(*two_running_linux_mediaservers)
    return two_running_linux_mediaservers


@pytest.fixture()
def linux_mediaserver(linux_vm, mediaserver_factory):
    with mediaserver_factory.allocated_mediaserver('single', linux_vm) as mediaserver:
        yield mediaserver


@pytest.fixture()
def running_linux_mediaserver(linux_mediaserver):
    linux_mediaserver.start()
    setup_local_system(linux_mediaserver, {})
    return linux_mediaserver
