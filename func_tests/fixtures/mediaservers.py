import logging

from argparse import ArgumentTypeError
import pytest

from defaults import defaults
from framework.installation.installer import Installer, PackageNameParseError
from framework.installation.lightweight_mediaserver import LWS_BINARY_NAME
from framework.installation.mediaserver_factory import MediaserverFactory
from framework.merging import merge_systems, setup_local_system
from framework.os_access.local_path import LocalPath
from framework.os_access.path import copy_file
import framework.licensing as licensing

_logger = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption(
        '--mediaserver-installers-dir', default=defaults.get('mediaserver_installers_dir'), type=LocalPath,
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
        if installer.product != 'server':
            _logger.debug("File %s: not a 'server' (%r)", path, installer)
            continue
        _logger.info("File {}: {!r}".format(path, installer))
        installers.append(installer)
    if len(set((installer.identity.customization, installer.identity.version) for installer in installers)) != 1:
        raise ValueError("Only one version and customizations expected in {}: {}".format(installers_dir, installers))
    installers_by_platform = {installer.platform: installer for installer in installers}
    return installers_by_platform


# we are expecting only one appserver2_ut in --mediaserver-installers-dir, for linux-x64 platform
@pytest.fixture(scope='session')
def lightweight_mediaserver_installer(request):
    installers_dir = request.config.getoption('--mediaserver-installers-dir')  # type: LocalPath
    path = installers_dir / LWS_BINARY_NAME
    if not path.exists():
        raise ArgumentTypeError(
            '{} is missing from {}, but is required.'.format(LWS_BINARY_NAME, installers_dir))
    _logger.info("Ligheweight mediaserver installer path: {}".format(path))
    return path


@pytest.fixture()
def mediaserver_factory(mediaserver_installers, artifacts_dir, ca):
    return MediaserverFactory(mediaserver_installers, artifacts_dir, ca)


@pytest.fixture()
def two_stopped_mediaservers(mediaserver_factory, two_vms, artifacts_dir):
    first_vm, second_vm = two_vms
    with mediaserver_factory.allocated_mediaserver('first', first_vm) as first_mediaserver:
        with mediaserver_factory.allocated_mediaserver('second', second_vm) as second_mediaserver:
            with first_mediaserver.os_access.traffic_capture.capturing() as first_cap:
                with second_mediaserver.os_access.traffic_capture.capturing() as second_cap:
                    yield first_mediaserver, second_mediaserver
            copy_file(first_cap, artifacts_dir / 'first.cap')
            copy_file(second_cap, artifacts_dir / 'second.cap')


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
def one_mediaserver(one_vm, mediaserver_factory, artifacts_dir):
    with mediaserver_factory.allocated_mediaserver('single', one_vm) as mediaserver:
        with mediaserver.os_access.traffic_capture.capturing() as single_cap:
            yield mediaserver
        copy_file(single_cap, artifacts_dir / 'single.cap')


@pytest.fixture()
def one_running_mediaserver(one_mediaserver):
    one_mediaserver.start()
    setup_local_system(one_mediaserver, {})
    return one_mediaserver


@pytest.fixture(scope='session')
def required_licenses():
    return [dict(n_cameras=100)]


@pytest.fixture
def one_licensed_server(one_mediaserver, required_licenses):
    one_mediaserver.os_access.networking.static_dns(licensing.TEST_SERVER_IP, licensing.DNS)
    one_mediaserver.start()
    setup_local_system(one_mediaserver, {})

    server = licensing.ServerApi()
    for license in required_licenses:
        one_mediaserver.api.get('api/activateLicense', params=dict(key=server.generate(**license)))

    return one_mediaserver
