import pytest
from pathlib2 import Path

from framework.installation.mediaserver_deb import MediaserverDeb
from framework.installation.mediaserver_factory import MediaserverFactory
from framework.merging import merge_systems, setup_local_system


def pytest_addoption(parser):
    parser.addoption('--mediaserver-deb', type=Path)


@pytest.fixture(scope='session')
def mediaserver_packages(request):
    return {
        'deb': request.config.getoption('--mediaserver-deb').expanduser(),
        }


@pytest.fixture(scope='session')
def mediaserver_deb(mediaserver_packages, request):
    deb = MediaserverDeb(mediaserver_packages['deb'])
    customization_from_command_line = request.config.getoption('--customization')
    if customization_from_command_line is not None:
        if deb.customization.name != customization_from_command_line:
            raise Exception(
                "Customization {} provided by --customization option "
                "doesn't match customization {} from .deb file. "
                "This option is maintained for backward compatibility, "
                "either don't use it or make sure it matches .deb file.".format(
                    customization_from_command_line,
                    deb.customization))
    return deb


@pytest.fixture()
def mediaserver_factory(mediaserver_packages, artifact_factory, ca):
    return MediaserverFactory(mediaserver_packages, artifact_factory, ca)


@pytest.fixture()
def two_clean_mediaservers(mediaserver_factory, two_vms):
    first_vm, second_vm = two_vms
    with mediaserver_factory.allocated_mediaserver(first_vm) as first_mediaserver:
        with mediaserver_factory.allocated_mediaserver(second_vm) as second_mediaserver:
            yield first_mediaserver, second_mediaserver


@pytest.fixture()
def two_stopped_mediaservers(two_clean_mediaservers):
    for mediaserver in two_clean_mediaservers:
        mediaserver.stop()
    return two_clean_mediaservers


@pytest.fixture()
def two_separate_mediaservers(two_clean_mediaservers):
    for mediaserver in two_clean_mediaservers:
        setup_local_system(mediaserver, {})
    return two_clean_mediaservers


@pytest.fixture()
def two_merged_mediaservers(two_separate_mediaservers):
    merge_systems(*two_separate_mediaservers)
    return two_separate_mediaservers


@pytest.fixture()
def one_mediaserver(one_vm, mediaserver_factory):
    with mediaserver_factory.allocated_mediaserver('single', one_vm) as mediaserver:
        yield mediaserver


@pytest.fixture()
def one_running_mediaserver(one_mediaserver):
    one_mediaserver.start()
    setup_local_system(one_mediaserver, {})
    return one_mediaserver
