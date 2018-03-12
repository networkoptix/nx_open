import logging
import os
from collections import namedtuple

import pytest
from netaddr import IPAddress
from pathlib2 import Path

from network_layouts import get_layout
from test_utils.artifact import ArtifactFactory
from test_utils.ca import CA
from test_utils.camera import CameraFactory, SampleMediaFile
from test_utils.cloud_host import CloudAccountFactory, resolve_cloud_host_from_registry
from test_utils.config import TestParameter, TestsConfig
from test_utils.lightweight_servers_factory import LWS_BINARY_NAME, LightweightServersFactory
from test_utils.mediaserverdeb import MediaserverDeb
from test_utils.merging import merge_system
from test_utils.metrics_saver import MetricsSaver
from test_utils.networking import setup_networks
from test_utils.os_access import LocalAccess
from test_utils.serialize import load
from test_utils.server_factory import ServerFactory
from test_utils.server_physical_host import PhysicalInstallationCtl
from test_utils.ssh.access_manager import SSHAccessManager
from test_utils.ssh.config import SSHConfig
from test_utils.vm import Pool, Registry, VMConfiguration, VirtualBox

JUNK_SHOP_PLUGIN_NAME = 'junk-shop-db-capture'

DEFAULT_CLOUD_GROUP = 'test'

DEFAULT_WORK_DIR = '/tmp/funtest'
DEFAULT_MEDIASERVER_DIST_NAME = 'mediaserver.deb'  # in bin dir
DEFAULT_REST_API_FORWARDED_PORT_BASE = 17000

DEFAULT_VM_HOST_USER = 'root'
DEFAULT_VM_HOST_DIR = '/tmp/jenkins-test'

DEFAULT_MAX_LOG_WIDTH = 500

MEDIA_SAMPLE_PATH = 'sample.mkv'
MEDIA_STREAM_PATH = 'sample.testcam-stream.data'

log = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption('--cloud-group', default=DEFAULT_CLOUD_GROUP, help=(
        'Cloud group; cloud host for it will be requested from ireg.hdw.mx; '
        'default is %r' % DEFAULT_CLOUD_GROUP))
    parser.addoption('--autotest-email-password', help=(
        'Password for accessing service account via IMAP protocol. '
        'Used for activation cloud accounts for different cloud groups and customizations.'))
    parser.addoption('--work-dir', type=Path, default=DEFAULT_WORK_DIR, help=(
        'working directory for tests: all generated files will be placed there'))
    parser.addoption('--bin-dir', type=Path, help=(
        'directory with binary files for tests: '
        'debian distributive and media sample are expected there'))
    parser.addoption('--customization', help=(
        "Manufacturer name or 'default'. Optional. Checked against customization from .deb file."))
    parser.addoption('--mediaserver-dist-path', type=Path, default=DEFAULT_MEDIASERVER_DIST_NAME, help=(
        'mediaserver package, relative to bin dir [%s]' % DEFAULT_MEDIASERVER_DIST_NAME))
    parser.addoption('--media-sample-path', type=Path, default=MEDIA_SAMPLE_PATH, help=(
        'media sample file path, default is %s at binary directory' % MEDIA_SAMPLE_PATH))
    parser.addoption('--media-stream-path', type=Path, default=MEDIA_STREAM_PATH, help=(
        'media sample test camera stream, relative to bin dir [%s]' % MEDIA_STREAM_PATH))
    parser.addoption('--vm-name-prefix', help=(
        'Deprecated. Left for backward compatibility. Ignored.'))
    parser.addoption('--vm-port-base', type=int, default=DEFAULT_REST_API_FORWARDED_PORT_BASE, help=(
        'base REST API port forwarded to host'))
    parser.addoption('--vm-address', type=IPAddress, help=(
        'IP address virtual machines bind to. '
        'Test camera discovery will answer only to this address if this option is specified.'))
    parser.addoption('--vm-host', help=(
        'hostname or IP address for host with VirtualBox, '
        'used to start virtual machines (by default it is local host)'))
    parser.addoption('--vm-host-user', default=DEFAULT_VM_HOST_USER, help=(
        'User to use for ssh to login to VirtualBox host'))
    parser.addoption('--vm-host-key', help=(
        'Identity file to use for ssh to login to VirtualBox host'))
    parser.addoption('--vm-host-dir', default=DEFAULT_VM_HOST_DIR, help=(
        'Working directory at host with VirtualBox, used to store vagrant files'))
    parser.addoption('--max-log-width', default=DEFAULT_MAX_LOG_WIDTH, type=int, help=(
        'Change maximum log message width. Default is %d' % DEFAULT_MAX_LOG_WIDTH))
    parser.addoption(
        '--log-level', type=str.upper, choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'], default='DEBUG',
        help='choices: DEBUG (default), INFO, WARNING, ERROR, CRITICAL')
    parser.addoption('--tests-config-file', type=TestsConfig.from_yaml_file, nargs='*', help=(
        'Configuration file for tests, in yaml format.'))
    parser.addoption('--test-parameters', type=TestParameter.from_str, help=(
        'Configuration parameters for a test, format: --test-parameter=test.param1=value1,test.param2=value2'))
    parser.addoption('--clean', '--reinstall', action='store_true', help='destroy VMs first')


@pytest.fixture(scope='session')
def vm_address(request):
    return request.config.getoption('--vm-address'),


VMHost = namedtuple('VMHost', ['hostname', 'username', 'private_key', 'work_dir', 'vm_port_base', 'vm_name_prefix'])


@pytest.fixture(scope='session')
def vm_host(request):
    return VMHost(
        hostname=request.config.getoption('--vm-host'),
        work_dir=request.config.getoption('--vm-host-dir'),
        vm_name_prefix=request.config.getoption('--vm-name-prefix'),
        vm_port_base=request.config.getoption('--vm-port-base'),
        username=request.config.getoption('--vm-host-user'),
        private_key=request.config.getoption('--vm-host-key'))


@pytest.fixture(scope='session')
def ssh_config(work_dir, vm_host):
    config = SSHConfig(work_dir / 'ssh.config')
    config.reset()
    if vm_host.hostname:
        config.add_host(
            vm_host.hostname,
            user=vm_host.username,
            key_path=vm_host.private_key)
    return config


@pytest.fixture(scope='session')
def work_dir(request):
    work_dir = request.config.getoption('--work-dir').expanduser()
    work_dir.mkdir(exist_ok=True, parents=True)
    return work_dir


@pytest.fixture(scope='session')
def bin_dir(request):
    bin_dir = request.config.getoption('--bin-dir').expanduser()
    assert bin_dir, 'Argument --bin-dir is required'
    return bin_dir


@pytest.fixture(scope='session')
def ca(work_dir):
    return CA(work_dir / 'ca')


@pytest.fixture(scope='session')
def mediaserver_deb(request, bin_dir):
    deb = MediaserverDeb(bin_dir / request.config.getoption('--mediaserver-dist-path'))
    customization_from_command_line = request.config.getoption('--customization')
    if customization_from_command_line is not None:
        if deb.customization.name != customization_from_command_line:
            raise Exception(
                "Customization {!r} provided by --customization option "
                "doesn't match customization {!r} from .deb file. "
                "This option is maintained for backward compatibility, "
                "either don't use it or make sure it matches .deb file.".format(
                    customization_from_command_line,
                    deb.customization))
    return deb


@pytest.fixture(scope='session', autouse=True)
def init_logging(request):
    # format = '%(asctime)-15s %(threadName)s %(name)s %(levelname)s  %(message)s'
    # %.10s limits formatted string to 10 chars; %(text).10s makes the same for dict-style formatting
    max_log_width = request.config.getoption('--max-log-width'),
    format = '%%(asctime)-15s %%(threadName)-15s %%(levelname)-7s %%(message).%ds' % max_log_width
    logging.basicConfig(level=request.config.getoption('--log-level'), format=format)


@pytest.fixture(scope='session')
def test_config(request):
    return TestsConfig.merge_config_list(
        request.config.getoption('--tests-config-file'),
        request.config.getoption('--test-parameters'))


@pytest.fixture()
def junk_shop_repository(request):
    db_capture_plugin = request.config.pluginmanager.getplugin(JUNK_SHOP_PLUGIN_NAME)
    if db_capture_plugin:
        db_capture_repository = db_capture_plugin.repo
        current_test_run = db_capture_plugin.current_test_run
        assert current_test_run
    else:
        db_capture_repository = None
        current_test_run = None
    return db_capture_repository, current_test_run


@pytest.fixture()
def artifact_factory(request, work_dir, junk_shop_repository):
    db_capture_repository, current_test_run = junk_shop_repository
    test_file_path_as_str, test_function_name = request.node.nodeid.split('::', 1)
    test_file_stem = Path(test_file_path_as_str).stem  # Name without extension.
    artifact_path_prefix = work_dir / (test_file_stem + '-' + test_function_name.replace('/', '.'))
    artifact_set = set()
    artifact_factory = ArtifactFactory.from_path(db_capture_repository, current_test_run, artifact_set,
                                                 artifact_path_prefix)
    yield artifact_factory
    artifact_factory.release()


@pytest.fixture()
def metrics_saver(junk_shop_repository):
    db_capture_repository, current_test_run = junk_shop_repository
    if not db_capture_repository:
        log.warning('Junk shop plugin is not available; No metrics will be saved')
    return MetricsSaver(db_capture_repository, current_test_run)


@pytest.fixture(params=['http', 'https'])
def http_schema(request):
    return request.param


@pytest.fixture(params=['rtsp', 'webm', 'hls', 'direct-hls'])
def stream_type(request):
    return request.param


@pytest.fixture()
def cloud_host(request, mediaserver_deb):
    cloud_group = request.config.getoption('--cloud-group'),
    return resolve_cloud_host_from_registry(cloud_group, mediaserver_deb.customization.company)


@pytest.fixture(scope='session')
def configuration():
    return load(Path(__file__).with_name('configuration.yaml').read_text())


@pytest.fixture()
def vm_pools(request, configuration, ssh_config):
    # All objects here are initialized quickly.
    # After pool has been closed, it's possible to reuse it, but, for simplicity, it's recreated.
    host_os_access = LocalAccess()
    registry = Registry(host_os_access, host_os_access.expand_path(configuration['vm_host']['registry']), 100)
    access_manager = SSHAccessManager(ssh_config, 'root', Path(configuration['ssh']['private_key']).expanduser())
    hypervisor = VirtualBox(host_os_access, configuration['vm_host']['address'])
    pools = {
        vm_type: Pool(VMConfiguration(vm_configuration_raw), registry, hypervisor, access_manager)
        for vm_type, vm_configuration_raw
        in configuration['vm_types'].items()}
    if request.config.getoption('--clean'):
        pools['linux'].destroy()
    yield pools
    for pool in pools.values():
        pool.close()


@pytest.fixture()
def linux_vm_pool(vm_pools):
    return vm_pools['linux']


@pytest.fixture(scope='session')
def physical_installation_ctl(test_config, ca, mediaserver_deb):
    if not test_config:
        return None
    pic = PhysicalInstallationCtl(
        mediaserver_deb.path,
        mediaserver_deb.customization.company,
        test_config.physical_installation_host_list,
        ca)
    return pic


@pytest.fixture()
def server_factory(mediaserver_deb, ca, artifact_factory, cloud_host, vm_pools, physical_installation_ctl):
    server_factory = ServerFactory(
        artifact_factory,
        cloud_host,
        vm_pools['linux'],
        physical_installation_ctl,
        mediaserver_deb,
        ca,
        )
    yield server_factory
    server_factory.release()


@pytest.fixture()
def lightweight_servers_factory(bin_dir, ca, artifact_factory, physical_installation_ctl):
    test_binary_path = bin_dir / LWS_BINARY_NAME
    assert test_binary_path.exists(), 'Test binary for lightweight servers is missing at %s' % test_binary_path
    lightweight_factory = LightweightServersFactory(artifact_factory, physical_installation_ctl, test_binary_path, ca)
    yield lightweight_factory
    lightweight_factory.release()


@pytest.fixture()
def cloud_account_factory(request, mediaserver_deb, cloud_host):
    return CloudAccountFactory(
        request.config.getoption('--cloud-group'),
        mediaserver_deb.customization.company,
        cloud_host,
        request.config.getoption('--autotest-email-password') or os.environ.get('AUTOTEST_EMAIL_PASSWORD'))


# CloudAccount instance
@pytest.fixture()
def cloud_account(cloud_account_factory):
    return cloud_account_factory()


@pytest.fixture()
def camera_factory(request, bin_dir, vm_address):
    stream_path = bin_dir / request.config.getoption('--media-stream-path'),
    assert stream_path.exists(), '%s is expected at %s' % (stream_path.name, stream_path.parent)
    factory = CameraFactory(vm_address, stream_path)
    yield factory
    factory.close()


@pytest.fixture()
def camera(camera_factory):
    return camera_factory()


@pytest.fixture()
def sample_media_file(request, bin_dir):
    path = bin_dir / request.config.getoption('--media-sample-path'),
    assert path.exists(), '%s is expected at %s' % (path.name, path.parent)
    return SampleMediaFile(path)


@pytest.fixture()
def network(vm_pools, server_factory, layout_file):
    layout = get_layout(layout_file)
    vms = setup_networks(vm_pools, layout.networks)
    servers = merge_system(server_factory, layout.mergers)
    return vms, servers


# pytest teardown does not allow failing the test from it. We have to use pytest hook for this.
@pytest.mark.hookwrapper
def pytest_pyfunc_call(pyfuncitem):
    # look up for server factory fixture()
    server_factory = None
    lws_factory = None
    # noinspection PyProtectedMember
    request = pyfuncitem._request
    for name in request.fixturenames:
        value = request.getfixturevalue(name)
        if isinstance(value, ServerFactory):
            server_factory = value
        if isinstance(value, LightweightServersFactory):
            lws_factory = value
    # run the test
    outcome = yield
    # perform post-checks if passed and have our server factory in fixtures
    passed = outcome.excinfo is None
    if passed and server_factory:
        server_factory.perform_post_checks()
    if passed and lws_factory:
        lws_factory.perform_post_checks()
