import logging

import pytest

from framework.installation.installer import Installer, PackageNameParseError
from framework.installation.mediaserver_factory import MediaserverFactory
from framework.merging import merge_systems, setup_local_system
from framework.os_access.local_path import LocalPath

_logger = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption(
        '--mediaserver-installers-dir', type=LocalPath, required=True,
        help="Directory with installers of same version and customization.")
    parser.addoption('--mediaserver-dist-path', help="Ignored.")


@pytest.fixture(scope='session')
def mediaserver_installers(request):
    installers = []
    installers_dir = request.config.getoption('--mediaserver-installers-dir')  # type: LocalPath
    for path in installers_dir.glob('*'):
        try:
            installer = Installer(path)
        except PackageNameParseError as e:
            _logger.debug("File {}: {!s}".format(path, e.message))
            continue
        _logger.info("File {}: {!r}".format(path, installer))
        installers.append(installer)
    if len(set((installer.customization, installer.version) for installer in installers)) != 1:
        raise ValueError("Only one version and customizations expected in {}: {}".format(installers_dir, installers))
    installers_by_platform = {installer.platform: installer for installer in installers}
    return installers_by_platform


@pytest.fixture()
def mediaserver_factory(mediaserver_installers, artifact_factory, ca):
    return MediaserverFactory(mediaserver_installers, artifact_factory, ca)


@pytest.fixture()
def two_stopped_mediaservers(mediaserver_factory, two_vms):
    first_vm, second_vm = two_vms
    with mediaserver_factory.allocated_mediaserver('first', first_vm) as first_mediaserver:
        with mediaserver_factory.allocated_mediaserver('second', second_vm) as second_mediaserver:
            yield first_mediaserver, second_mediaserver


@pytest.fixture()
def two_clean_mediaservers(two_stopped_mediaservers):
    for mediaserver in two_stopped_mediaservers:
        mediaserver.start()
    return two_stopped_mediaservers


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
