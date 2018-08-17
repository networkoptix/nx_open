import logging

import pytest

import framework.licensing as licensing
from defaults import defaults
from framework.installation.installer import Installer, PackageNameParseError
from framework.installation.mediaserver_factory import allocated_mediaserver
from framework.merging import merge_systems
from framework.os_access.local_path import LocalPath

_logger = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption(
        '--mediaserver-installers-dir', default=defaults.get('mediaserver_installers_dir'), type=LocalPath,
        help="Directory with installers of same version and customization.")
    parser.addoption(
        '--customization',
        help="Customization name. Only checked against customization of installer.")
    parser.addoption('--mediaserver-dist-path', help="Ignored.")


@pytest.fixture(scope='session')
def mediaserver_installers_dir(request):
    return request.config.getoption('--mediaserver-installers-dir')  # type: LocalPath


@pytest.fixture(scope='session')
def mediaserver_installers(mediaserver_installers_dir):
    installers = []
    for path in mediaserver_installers_dir.glob('*'):
        try:
            installer = Installer(path)
        except PackageNameParseError as e:
            _logger.debug("File {}: {!s}".format(path, e.message))
            continue
        _logger.info("File {}: {!r}".format(path, installer))
        installers.append(installer)
    if len(set((installer.identity.customization, installer.identity.version) for installer in installers)) != 1:
        raise ValueError("Only one version and customizations expected in {}: {}".format(mediaserver_installers_dir, installers))
    installers_by_platform = {installer.platform: installer for installer in installers}
    return installers_by_platform


@pytest.fixture(scope='session')
def customization(request, mediaserver_installers):
    customization, = {installer.customization for installer in mediaserver_installers.values()}
    customization_from_argument = request.config.getoption('--customization')
    if customization_from_argument is not None:
        if customization_from_argument != customization.name:
            raise ValueError(
                "Customization name {!r} provided via --customization doesn't match {!r} of {!r}".format(
                    customization_from_argument, mediaserver_installers, customization))
    return customization


@pytest.fixture()
def two_stopped_mediaservers(mediaserver_installers, artifacts_dir, ca, two_vms):
    first_vm, second_vm = two_vms
    with allocated_mediaserver(
            mediaserver_installers, artifacts_dir,
            ca, 'first', first_vm) as first_mediaserver:
        with allocated_mediaserver(
                mediaserver_installers, artifacts_dir,
                ca, 'second', second_vm) as second_mediaserver:
            yield first_mediaserver, second_mediaserver


@pytest.fixture()
def two_clean_mediaservers(two_stopped_mediaservers):
    for mediaserver in two_stopped_mediaservers:
        mediaserver.start()
    return two_stopped_mediaservers


@pytest.fixture()
def two_separate_mediaservers(two_clean_mediaservers):
    for mediaserver in two_clean_mediaservers:
        mediaserver.api.setup_local_system()
    return two_clean_mediaservers


@pytest.fixture()
def two_merged_mediaservers(two_separate_mediaservers):
    merge_systems(*two_separate_mediaservers)
    return two_separate_mediaservers


@pytest.fixture()
def one_mediaserver(mediaserver_installers, artifacts_dir, ca, one_vm):
    with allocated_mediaserver(
            mediaserver_installers, artifacts_dir,
            ca, 'single', one_vm) as mediaserver:
        yield mediaserver


@pytest.fixture()
def one_running_mediaserver(one_mediaserver):
    one_mediaserver.start()
    one_mediaserver.api.setup_local_system()
    return one_mediaserver


@pytest.fixture(scope='session')
def required_licenses():
    return [dict(n_cameras=100)]


@pytest.fixture
def one_licensed_mediaserver(one_mediaserver, required_licenses):
    one_mediaserver.os_access.networking.static_dns(licensing.TEST_SERVER_IP, licensing.DNS)
    one_mediaserver.start()
    one_mediaserver.api.setup_local_system()

    server = licensing.ServerApi()
    for license in required_licenses:
        one_mediaserver.api.generic.get('api/activateLicense', params=dict(key=server.generate(**license)))

    return one_mediaserver
