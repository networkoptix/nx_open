'''pytest configuration file for functional testsevi

Loaded by pytest before running all functional tests. Adds common fixtures used by tests.
'''

import sys
import os.path
import logging
import pytest
from netaddr import IPAddress
from test_utils.utils import SimpleNamespace
from test_utils.config import TestParameter, TestsConfig, SingleTestConfig
from test_utils.artifact import ArtifactFactory
from test_utils.customization import read_customization_company_name
from test_utils.host import SshHostConfig
from test_utils.vagrant_box_config import BoxConfigFactory
from test_utils.vagrant_box import VagrantBoxFactory
from test_utils.server_physical_host import PhysicalInstallationCtl
from test_utils.cloud_host import resolve_cloud_host_from_registry, create_cloud_host
from test_utils.server_factory import ServerFactory
from test_utils.camera import SampleMediaFile, CameraFactory


JUNK_SHOP_PLUGIN_NAME = 'junk-shop-db-capture'

DEFAULT_CLOUD_GROUP = 'test'
DEFAULT_CUSTOMIZATION = 'default'

DEFAULT_WORK_DIR = os.path.expanduser('/tmp/funtest')

DEFAULT_VM_NAME_PREFIX = 'funtest-'
DEFAULT_REST_API_FORWARDED_PORT_BASE = 17000

DEFAULT_VM_HOST_USER = 'root'
DEFAULT_VM_HOST_DIR = '/tmp/jenkins-test'

DEFAULT_MAX_LOG_WIDTH = 500

MEDIA_SAMPLE_FPATH = 'sample.mkv'
MEDIA_STREAM_FPATH = 'sample.testcam-stream.data'


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
    parser.addoption('--work-dir', default=DEFAULT_WORK_DIR,
                     help='working directory for tests: all generated files will be placed there')
    parser.addoption('--bin-dir',
                     help='directory with binary files for tests:'
                          ' debian distributive and media sample are expected there')
    parser.addoption('--media-sample-path', default=MEDIA_SAMPLE_FPATH,
                     help='media sample file path, default is %s at binary directory' % MEDIA_SAMPLE_FPATH)
    parser.addoption('--media-stream-path', default=MEDIA_STREAM_FPATH,
                     help='media sample test camera stream file path, default is %s at binary directory' % MEDIA_STREAM_FPATH)
    parser.addoption('--no-servers-reset', action='store_true',
                     help='skip servers reset/cleanup on test setup')
    parser.addoption('--recreate-boxes', action='store_true', help='destroy and create again vagrant boxes')
    parser.addoption('--reinstall', action='store_true',
                     help='Take and install new distrubutive.'
                     ' Recreate all vagrant boxes and reinstall server on physical servers.')
    parser.addoption('--vm-name-prefix', default=DEFAULT_VM_NAME_PREFIX,
                     help='prefix for virtualenv machine names')
    parser.addoption('--vm-port-base', type=int, default=DEFAULT_REST_API_FORWARDED_PORT_BASE,
                     help='base REST API port forwarded to host')
    parser.addoption('--vm-address', type=IPAddress,
                     help='IP address virtual machines bind to.'
                     ' Test camera discovery will answer only to this address if this option is specified.')
    parser.addoption('--vm-host',
                     help='hostname or IP address for host with virtualbox,'
                          ' used to start virtual machines (by default it is local host)')
    parser.addoption('--vm-host-user', default=DEFAULT_VM_HOST_USER,
                     help='User to use for ssh to login to virtualbox host')
    parser.addoption('--vm-host-key',
                     help='Identity file to use for ssh to login to virtualbox host')
    parser.addoption('--vm-host-dir', default=DEFAULT_VM_HOST_DIR,
                     help='Working directory at host with virtualbox, used to store vagrant files')
    parser.addoption('--max-log-width', default=DEFAULT_MAX_LOG_WIDTH, type=int,
                     help='Change maximum log message width. Default is %d' % DEFAULT_MAX_LOG_WIDTH)
    parser.addoption('--log-level', default=log_levels[0], type=str.upper,
                     choices=log_levels,
                     help='Change log level (%s). Default is %s' % (', '.join(log_levels), log_levels[0]))
    parser.addoption('--tests-config-file', type=TestsConfig.from_yaml_file, nargs='*',
                     help='Configuration file for tests, in yaml format.')
    parser.addoption('--test-parameter', type=TestParameter.from_str, nargs='*',
                     help='Configuration parameter for a test, in format: --test-parameter=test.param=value')


@pytest.fixture(scope='session')
def run_options(request):
    vm_host = request.config.getoption('--vm-host')
    if vm_host:
        vm_ssh_host_config = SshHostConfig(
            host=vm_host,
            user=request.config.getoption('--vm-host-user'),
            key_file_path=request.config.getoption('--vm-host-key'))
    else:
        vm_ssh_host_config = None
    bin_dir = request.config.getoption('--bin-dir')
    assert bin_dir, 'Argument --bin-dir is required'
    tests_config = TestsConfig.merge_config_list(request.config.getoption('--tests-config-file'))
    tests_config.update_with_tests_params(request.config.getoption('--test-parameter'))
    return SimpleNamespace(
        cloud_group=request.config.getoption('--cloud-group'),
        customization=request.config.getoption('--customization'),
        work_dir=request.config.getoption('--work-dir'),
        bin_dir=request.config.getoption('--bin-dir'),
        media_sample_path=request.config.getoption('--media-sample-path'),
        media_stream_path=request.config.getoption('--media-stream-path'),
        reset_servers=not request.config.getoption('--no-servers-reset'),
        recreate_boxes=request.config.getoption('--recreate-boxes'),
        reinstall=request.config.getoption('--reinstall'),
        vm_name_prefix=request.config.getoption('--vm-name-prefix'),
        vm_port_base=request.config.getoption('--vm-port-base'),
        vm_address=request.config.getoption('--vm-address'),
        vm_ssh_host_config=vm_ssh_host_config,
        vm_host_work_dir=request.config.getoption('--vm-host-dir'),
        max_log_width=request.config.getoption('--max-log-width'),
        log_level=request.config.getoption('--log-level'),
        tests_config=tests_config,
        )


@pytest.fixture(scope='session')
def init_logging(run_options):
    #format = '%(asctime)-15s %(threadName)s %(name)s %(levelname)s  %(message)s'
    # %.10s limits formatted string to 10 chars; %(text).10s makes the same for dict-style formatting
    format = '%%(asctime)-15s %%(threadName)-15s %%(levelname)-7s %%(message).%ds' % run_options.max_log_width
    logging.basicConfig(level=run_options.log_level, format=format)

@pytest.fixture
def test_config(request, run_options):
    if run_options.tests_config:
        return run_options.tests_config.get_test_config(request.node.nodeid)
    else:
        return SingleTestConfig()

@pytest.fixture
def artifact_factory(request, run_options):
    db_capture_plugin = request.config.pluginmanager.getplugin(JUNK_SHOP_PLUGIN_NAME)
    if db_capture_plugin:
        db_capture_repository = db_capture_plugin.repo
        current_test_run = db_capture_plugin.current_test_run
        assert current_test_run
    else:
        db_capture_repository = None
        current_test_run = None
    artifact_path_prefix = os.path.join(
        run_options.work_dir,
        os.path.basename(request.node.nodeid.replace('::', '-').replace('.py', '')))
    artifact_set = set()
    artifact_factory = ArtifactFactory.from_path(db_capture_repository, current_test_run, artifact_set, artifact_path_prefix)
    yield artifact_factory
    artifact_factory.release()


@pytest.fixture(scope='session')
def customization_company_name(run_options):
    return read_customization_company_name(run_options.customization)


@pytest.fixture(params=['http', 'https'])
def http_schema(request):
    return request.param


@pytest.fixture(params=['rtsp', 'webm', 'hls', 'direct-hls'])
def stream_type(request):
    return request.param

# cloud host dns name, like: 'cloud-dev.hdw.mx'
@pytest.fixture
def cloud_host_host(init_logging, run_options):
    return resolve_cloud_host_from_registry(run_options.cloud_group, run_options.customization)

@pytest.fixture(scope='session')
def box_factory(request, run_options, init_logging, customization_company_name):
    config_factory = BoxConfigFactory(customization_company_name)
    factory = VagrantBoxFactory(
        request.config.cache,
        run_options,
        config_factory,
        )
    if run_options.recreate_boxes or run_options.reinstall:
        factory.destroy_all()
    return factory

@pytest.fixture
def box(box_factory):
    yield box_factory
    box_factory.release_all_boxes()

@pytest.fixture(scope='session')
def physical_installation_ctl(run_options, init_logging, customization_company_name):
    if not run_options.tests_config:
        return None
    pic = PhysicalInstallationCtl(
        run_options.bin_dir,
        customization_company_name,
        run_options.tests_config.physical_installation_host_list,
        )
    if run_options.reinstall:
        pic.clean_all_installations()
    return pic

@pytest.fixture
def server_factory(run_options, init_logging, artifact_factory, customization_company_name,
                   cloud_host_host, box, physical_installation_ctl):
    server_factory = ServerFactory(
        run_options.reset_servers,
        artifact_factory,
        customization_company_name,
        cloud_host_host,
        box,
        physical_installation_ctl)
    yield server_factory
    server_factory.release()

# CloudHost instance    
@pytest.fixture
def cloud_host(run_options, cloud_host_host):
    cloud_host = create_cloud_host(run_options.cloud_group, run_options.customization, cloud_host_host)
    cloud_host.check_is_ready_for_tests()
    return cloud_host


@pytest.fixture
def camera_factory(run_options, init_logging):
    stream_path = os.path.abspath(os.path.join(run_options.bin_dir, run_options.media_stream_path))
    assert os.path.isfile(stream_path), '%s is expected at %s' % (os.path.basename(stream_path), os.path.dirname(stream_path))
    factory = CameraFactory(run_options.vm_address, stream_path)
    yield factory
    factory.close()

@pytest.fixture
def camera(camera_factory):
    return camera_factory()


@pytest.fixture
def sample_media_file(run_options):
    fpath = os.path.abspath(os.path.join(run_options.bin_dir, run_options.media_sample_path))
    assert os.path.isfile(fpath), '%s is expected at %s' % (os.path.basename(fpath), os.path.dirname(fpath))
    return SampleMediaFile(fpath)


# pytest teardown does not allow failing the test from it. We have to use pytest hook for this.
@pytest.mark.hookwrapper
def pytest_pyfunc_call(pyfuncitem):
    # look up for server factory fixture
    server_factory = None
    for name in pyfuncitem._request.fixturenames:
        value = pyfuncitem._request.getfixturevalue(name)
        if isinstance(value, ServerFactory):
            server_factory = value
    # run the test
    outcome = yield
    # perform post-checks if passed and have our server factory in fixtures
    passed = outcome.excinfo is None
    if passed and server_factory:
        server_factory.perform_post_checks()
