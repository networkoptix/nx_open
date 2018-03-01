import logging
import os
from textwrap import dedent

import pytest
import yaml
from netaddr import IPAddress, IPNetwork
from pathlib2 import Path

from test_utils.artifact import ArtifactFactory
from test_utils.ca import CA
from test_utils.camera import CameraFactory, SampleMediaFile
from test_utils.cloud_host import CloudAccountFactory, resolve_cloud_host_from_registry
from test_utils.config import SingleTestConfig, TestParameter, TestsConfig
from test_utils.lightweight_servers_factory import LWS_BINARY_NAME, LightweightServersFactory
from test_utils.mediaserverdeb import MediaserverDeb
from test_utils.metrics_saver import MetricsSaver
from test_utils.networking import setup_networks
from test_utils.server_factory import ServerFactory
from test_utils.server_physical_host import PhysicalInstallationCtl
from test_utils.ssh.config import SSHConfig
from test_utils.utils import SimpleNamespace
from test_utils.vagrant_vm import VagrantVMFactory
from test_utils.vagrant_vm_config import VagrantVMConfigFactory

JUNK_SHOP_PLUGIN_NAME = 'junk-shop-db-capture'

DEFAULT_CLOUD_GROUP = 'test'
DEFAULT_CUSTOMIZATION = 'default'

DEFAULT_WORK_DIR = '/tmp/funtest'
DEFAULT_MEDIASERVER_DIST_NAME = 'mediaserver.deb'  # in bin dir
DEFAULT_VIRTUALBOX_NAME_PREFIX = 'vagrant_func_tests-'  # Default Vagrant's prefix is "vagrant_".
DEFAULT_REST_API_FORWARDED_PORT_BASE = 17000

DEFAULT_VM_HOST_USER = 'root'
DEFAULT_VM_HOST_DIR = '/tmp/jenkins-test'

DEFAULT_MAX_LOG_WIDTH = 500

MEDIA_SAMPLE_PATH = 'sample.mkv'
MEDIA_STREAM_PATH = 'sample.testcam-stream.data'

log = logging.getLogger(__name__)


def pytest_addoption(parser):
    log_levels = [logging.getLevelName(logging.DEBUG),
                  logging.getLevelName(logging.INFO),
                  logging.getLevelName(logging.WARNING),
                  logging.getLevelName(logging.ERROR),
                  logging.getLevelName(logging.CRITICAL)]
    parser.addoption('--cloud-group', default=DEFAULT_CLOUD_GROUP,
                     help='Cloud group; cloud host for it will be requested from ireg.hdw.mx;'
                          ' default is %r' % DEFAULT_CLOUD_GROUP)
    parser.addoption('--customization', default=DEFAULT_CUSTOMIZATION,
                     help='Customization; will be used for requesting cloud host from ireg.hdw.mx;'
                          ' default is %r' % DEFAULT_CUSTOMIZATION)
    parser.addoption('--autotest-email-password',
                     help='Password for accessing service account via IMAP protocol. '
                          'Used for activation cloud accounts for different cloud groups and customizations.')
    parser.addoption('--work-dir', default=DEFAULT_WORK_DIR, type=Path,
                     help='working directory for tests: all generated files will be placed there')
    parser.addoption('--bin-dir', type=Path,
                     help='directory with binary files for tests:'
                          ' debian distributive and media sample are expected there')
    parser.addoption('--mediaserver-dist-path', type=Path, default=DEFAULT_MEDIASERVER_DIST_NAME,
                     help='mediaserver package, relative to bin dir [%s]' % DEFAULT_MEDIASERVER_DIST_NAME)
    parser.addoption('--media-sample-path', default=MEDIA_SAMPLE_PATH, type=Path,
                     help='media sample file path, default is %s at binary directory' % MEDIA_SAMPLE_PATH)
    parser.addoption('--media-stream-path', default=MEDIA_STREAM_PATH, type=Path,
                     help='media sample test camera stream, relative to bin dir [%s]' % MEDIA_STREAM_PATH)
    parser.addoption('--no-servers-reset', action='store_true',
                     help='skip servers reset/cleanup on test setup')
    parser.addoption('--recreate-vms', action='store_true', help='destroy and create again vagrant VMs')
    parser.addoption('--vm-name-prefix', default=DEFAULT_VIRTUALBOX_NAME_PREFIX,
                     help='prefix for virtualenv machine names')
    parser.addoption('--vm-port-base', type=int, default=DEFAULT_REST_API_FORWARDED_PORT_BASE,
                     help='base REST API port forwarded to host')
    parser.addoption('--vm-address', type=IPAddress,
                     help='IP address virtual machines bind to.'
                          ' Test camera discovery will answer only to this address if this option is specified.')
    parser.addoption('--vm-host',
                     help='hostname or IP address for host with VirtualBox,'
                          ' used to start virtual machines (by default it is local host)')
    parser.addoption('--vm-host-user', default=DEFAULT_VM_HOST_USER,
                     help='User to use for ssh to login to VirtualBox host')
    parser.addoption('--vm-host-key',
                     help='Identity file to use for ssh to login to VirtualBox host')
    parser.addoption('--vm-host-dir', default=DEFAULT_VM_HOST_DIR,
                     help='Working directory at host with VirtualBox, used to store vagrant files')
    parser.addoption('--max-log-width', default=DEFAULT_MAX_LOG_WIDTH, type=int,
                     help='Change maximum log message width. Default is %d' % DEFAULT_MAX_LOG_WIDTH)
    parser.addoption('--log-level', default=log_levels[0], type=str.upper,
                     choices=log_levels,
                     help='Change log level (%s). Default is %s' % (', '.join(log_levels), log_levels[0]))
    parser.addoption('--tests-config-file', type=TestsConfig.from_yaml_file, nargs='*',
                     help='Configuration file for tests, in yaml format.')
    parser.addoption('--test-parameters', type=TestParameter.from_str, help=dedent('''
                         Configuration parameters for a test, format: 
                         --test-parameter=test.param1=value1,test.param2=value2
                         ''').strip())


@pytest.fixture(scope='session')
def run_options(request):
    work_dir = request.config.getoption('--work-dir').expanduser()
    work_dir.mkdir(exist_ok=True, parents=True)
    common_ssh_config = SSHConfig(work_dir / 'ssh.config')
    common_ssh_config.reset()
    vm_host_hostname = request.config.getoption('--vm-host')
    if vm_host_hostname:
        common_ssh_config.add_host(
            vm_host_hostname,
            user=request.config.getoption('--vm-host-user'),
            key_path=request.config.getoption('--vm-host-key'))
    bin_dir = request.config.getoption('--bin-dir').expanduser()
    assert bin_dir, 'Argument --bin-dir is required'
    mediaserver_deb = MediaserverDeb(bin_dir / request.config.getoption('--mediaserver-dist-path'))
    autotest_email_password = request.config.getoption('--autotest-email-password') or os.environ.get(
        'AUTOTEST_EMAIL_PASSWORD')
    tests_config = TestsConfig.merge_config_list(
        request.config.getoption('--tests-config-file'),
        request.config.getoption('--test-parameters'),
        )
    return SimpleNamespace(
        cloud_group=request.config.getoption('--cloud-group'),
        customization=request.config.getoption('--customization'),
        autotest_email_password=autotest_email_password,
        work_dir=work_dir,
        bin_dir=bin_dir,
        mediaserver_deb=mediaserver_deb,
        media_sample_path=request.config.getoption('--media-sample-path'),
        media_stream_path=request.config.getoption('--media-stream-path'),
        reset_servers=not request.config.getoption('--no-servers-reset'),
        recreate_vms=request.config.getoption('--recreate-vms'),
        vm_name_prefix=request.config.getoption('--vm-name-prefix'),
        vm_port_base=request.config.getoption('--vm-port-base'),
        vm_address=request.config.getoption('--vm-address'),
        vm_host_hostname=vm_host_hostname,
        vm_host_work_dir=request.config.getoption('--vm-host-dir'),
        max_log_width=request.config.getoption('--max-log-width'),
        log_level=request.config.getoption('--log-level'),
        tests_config=tests_config,
        common_ssh_config=common_ssh_config,
        )


@pytest.fixture(scope='session')
def work_dir(run_options):
    return run_options.work_dir


@pytest.fixture(scope='session')
def bin_dir(run_options):
    return run_options.bin_dir


@pytest.fixture(scope='session')
def ca(work_dir):
    return CA(work_dir / 'ca')


@pytest.fixture(scope='session')
def mediaserver_deb(run_options):
    return run_options.mediaserver_deb


@pytest.fixture(scope='session', autouse=True)
def init_logging(run_options):
    # format = '%(asctime)-15s %(threadName)s %(name)s %(levelname)s  %(message)s'
    # %.10s limits formatted string to 10 chars; %(text).10s makes the same for dict-style formatting
    format = '%%(asctime)-15s %%(threadName)-15s %%(levelname)-7s %%(message).%ds' % run_options.max_log_width
    logging.basicConfig(level=run_options.log_level, format=format)


@pytest.fixture()
def test_config(request, run_options):
    if run_options.tests_config:
        return run_options.tests_config.get_test_config(request.node.nodeid)
    else:
        return SingleTestConfig()


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
def artifact_factory(request, run_options, junk_shop_repository):
    db_capture_repository, current_test_run = junk_shop_repository
    test_file_path_as_str, test_function_name = request.node.nodeid.split('::', 1)
    test_file_stem = Path(test_file_path_as_str).stem  # Name without extension.
    artifact_path_prefix = run_options.work_dir / (test_file_stem + '-' + test_function_name.replace('/', '.'))
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


@pytest.fixture(scope='session')
def customization_company_name(run_options):
    company_name = run_options.mediaserver_deb.customization.company_name
    log.info('Customization company name: %r', company_name)
    return company_name


@pytest.fixture(params=['http', 'https'])
def http_schema(request):
    return request.param


@pytest.fixture(params=['rtsp', 'webm', 'hls', 'direct-hls'])
def stream_type(request):
    return request.param


@pytest.fixture()
def cloud_host(run_options):
    return resolve_cloud_host_from_registry(run_options.cloud_group, run_options.customization)


@pytest.fixture(scope='session')
def session_vm_factory(request, run_options, customization_company_name):
    """Create factory once per session, don't release VMs"""
    config_factory = VagrantVMConfigFactory(customization_company_name)
    factory = VagrantVMFactory(
        request.config.cache,
        run_options.common_ssh_config,
        run_options.vm_host_hostname,
        run_options,
        config_factory,
        )
    return factory


@pytest.fixture()
def vm_factory(session_vm_factory):
    """Return same factory by release allocated VMs after each test"""
    yield session_vm_factory
    session_vm_factory.release_all_vms()


@pytest.fixture(scope='session')
def physical_installation_ctl(run_options, customization_company_name):
    if not run_options.tests_config:
        return None
    pic = PhysicalInstallationCtl(
        run_options.mediaserver_deb.path,
        customization_company_name,
        run_options.tests_config.physical_installation_host_list,
        CA(run_options.work_dir / 'ca'),
        )
    return pic


@pytest.fixture()
def server_factory(run_options, mediaserver_deb, ca, artifact_factory,
                   cloud_host, vm_factory, physical_installation_ctl):
    server_factory = ServerFactory(
        run_options.reset_servers,
        artifact_factory,
        cloud_host,
        vm_factory,
        physical_installation_ctl,
        mediaserver_deb,
        ca,
        )
    yield server_factory
    server_factory.release()


@pytest.fixture()
def lightweight_servers_factory(run_options, artifact_factory, physical_installation_ctl):
    test_binary_path = run_options.bin_dir / LWS_BINARY_NAME
    assert test_binary_path.exists(), 'Test binary for lightweight servers is missing at %s' % test_binary_path
    ca = CA(run_options.work_dir / 'ca')
    lightweight_factory = LightweightServersFactory(artifact_factory, physical_installation_ctl, test_binary_path, ca)
    yield lightweight_factory
    lightweight_factory.release()


@pytest.fixture()
def cloud_account_factory(run_options, cloud_host):
    return CloudAccountFactory(
        run_options.cloud_group, run_options.customization, cloud_host, run_options.autotest_email_password)


# CloudAccount instance
@pytest.fixture()
def cloud_account(cloud_account_factory):
    return cloud_account_factory()


@pytest.fixture()
def camera_factory(run_options):
    stream_path = run_options.bin_dir / run_options.media_stream_path
    assert stream_path.exists(), '%s is expected at %s' % (stream_path.name, stream_path.parent)
    factory = CameraFactory(run_options.vm_address, stream_path)
    yield factory
    factory.close()


@pytest.fixture()
def camera(camera_factory):
    return camera_factory()


@pytest.fixture()
def sample_media_file(run_options):
    path = run_options.bin_dir / run_options.media_sample_path
    assert path.exists(), '%s is expected at %s' % (path.name, path.parent)
    return SampleMediaFile(path)


@pytest.fixture()
def network(vm_factory, server_factory, layout_file):
    path = Path(__file__).with_name('network_layouts') / layout_file
    layout = yaml.load(path.read_text())
    setup_networks(vm_factory, layout['networks'])
    servers = {}
    for remote_alias, local_aliases in layout['mergers'].items():
        for local_alias, merge_parameters in local_aliases.items():
            servers[remote_alias] = server_factory.get(remote_alias)
            servers[local_alias] = server_factory.get(local_alias)
            merge_kwargs = {}
            if merge_parameters is not None:
                try:
                    merge_kwargs['take_remote_settings'] = merge_parameters['settings'] == 'remote'
                except KeyError:
                    pass
                try:
                    remote_network = IPNetwork(merge_parameters['network'])
                except KeyError:
                    pass
                else:
                    merge_kwargs['remote_network'] = remote_network
            servers[local_alias].merge_systems(servers[remote_alias], **merge_kwargs)
    return servers


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
