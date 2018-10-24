import logging
from contextlib import contextmanager

import pytest

import framework.licensing as licensing
from defaults import defaults
from framework.installation.installer import InstallerSet
from framework.installation.log_parser import parse_mediaserver_logs
from framework.installation.mediaserver import Mediaserver
from framework.merging import merge_systems
from framework.os_access.local_path import LocalPath

pytest_plugins = ['fixtures.vms']

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
def mediaserver_installers_dir(request, metadata):
    dir = request.config.getoption('--mediaserver-installers-dir')  # type: LocalPath
    metadata['Mediaserver Installers Dir'] = dir
    return dir


@pytest.fixture(scope='session')
def mediaserver_installer_set(mediaserver_installers_dir, metadata):
    installer_set = InstallerSet(mediaserver_installers_dir)
    metadata['Mediaserver Version'] = installer_set.version
    metadata['Mediaserver Customization'] = installer_set.customization.customization_name
    return installer_set


@pytest.fixture(scope='session')
def customization(request, mediaserver_installer_set):
    customization_from_argument = request.config.getoption('--customization')
    if customization_from_argument is not None:
        if customization_from_argument != mediaserver_installer_set.customization.customization_name:
            raise ValueError(
                "Customization name {!r} provided via --customization doesn't match {!r} of {!r}".format(
                    customization_from_argument,
                    mediaserver_installer_set, mediaserver_installer_set.customization))
    return mediaserver_installer_set.customization


@pytest.fixture()
def two_stopped_mediaservers(mediaserver_allocation, two_vms):
    first_vm, second_vm = two_vms
    with mediaserver_allocation(first_vm) as first_mediaserver:
        with mediaserver_allocation(second_vm) as second_mediaserver:
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
def one_mediaserver(mediaserver_allocation, one_vm):
    with mediaserver_allocation(one_vm) as mediaserver:
        yield mediaserver


@pytest.fixture()
def one_running_mediaserver(one_mediaserver):
    one_mediaserver.start()
    one_mediaserver.api.setup_local_system()
    return one_mediaserver


@pytest.fixture()
def one_mediaserver_api(one_running_mediaserver):
    return one_running_mediaserver.api


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
        one_mediaserver.api.generic.get(
            'api/activateLicense', params=dict(key=server.generate(**license)))

    return one_mediaserver


@pytest.fixture()
def mediaserver_allocation(mediaserver_installer_set, artifacts_dir, ca, elasticsearch):
    @contextmanager
    def cm(vm):
        mediaserver = Mediaserver.setup(
            vm.os_access, mediaserver_installer_set, ca.generate_key_and_cert())
        mediaserver_artifacts_dir = artifacts_dir / mediaserver.name
        mediaserver_artifacts_dir.ensure_empty_dir()
        capture_file = mediaserver_artifacts_dir / '{}.cap'.format(vm.alias)
        try:
            with mediaserver.os_access.traffic_capture.capturing(capture_file):
                yield mediaserver
        finally:
            mediaserver.examine()
            mediaserver.collect_artifacts(mediaserver_artifacts_dir)
            # TODO: Get logs only once.
            if elasticsearch is not None:
                for log_file in mediaserver.installation.list_log_files():
                    log_bytes = log_file.read_bytes()
                    static_data = {
                        'mediaserver': {
                            'name': mediaserver.name,
                            'api': mediaserver.api.generic.http.url(''),
                            'file': log_file.name,
                            },
                        }
                    parsed_log = parse_mediaserver_logs(log_bytes)
                    elasticsearch.bulk_upload('ft_mediaserver', static_data, parsed_log)

    return cm
