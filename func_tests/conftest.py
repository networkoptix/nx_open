'''pytest configuration file for functional tests

Loaded by pytest before running all functional tests. Adds common fixtures used by tests.
'''

import sys
import os.path
import logging
import pytest
from test_utils.utils import SimpleNamespace
from test_utils.session import TestSession
from test_utils.customization import read_customization_company_name
from test_utils.environment import EnvironmentBuilder
from test_utils.host import SshHostConfig
from test_utils.vagrant_box_config import BoxConfigFactory
from test_utils.cloud_host import resolve_cloud_host_from_registry, create_cloud_host
from test_utils.server import ServerConfigFactory
from test_utils.camera import MEDIA_SAMPLE_FPATH, SampleMediaFile, Camera


DEFAULT_CLOUD_GROUP = 'test'
DEFAULT_CUSTOMIZATION = 'default'

DEFAULT_WORK_DIR = os.path.expanduser('/tmp/funtest')
DEFAULT_BIN_DIR = os.path.expanduser('/tmp/binaries')

DEFAULT_VM_NAME_PREFIX = 'funtest-'

DEFAULT_VM_HOST_USER = 'root'
DEFAULT_VM_HOST_DIR = '/tmp/jenkins-test'

DEFAULT_MAX_LOG_WIDTH = 500


log = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption('--cloud-group', default=DEFAULT_CLOUD_GROUP,
                     help='Cloud group; cloud host for it will be requested from ireg.hdw.mx;'
                          ' default is %r' % DEFAULT_CLOUD_GROUP)
    parser.addoption('--customization', default=DEFAULT_CUSTOMIZATION,
                     help='Customization; will be used for requesting cloud host from ireg.hdw.mx;'
                          ' default is %r' % DEFAULT_CUSTOMIZATION)
    parser.addoption('--work-dir', default=DEFAULT_WORK_DIR,
                     help='working directory for tests: all generated files will be placed there')
    parser.addoption('--bin-dir', default=DEFAULT_BIN_DIR,
                     help='directory with binary files for tests:'
                          ' debian distributive and sample.mkv are expected there')
    parser.addoption('--no-servers-reset', action='store_true',
                     help='skip servers reset/cleanup on test setup')
    parser.addoption('--recreate-boxes', action='store_true',
                     help='destroy and create again vagrant boxes')
    parser.addoption('--vm-name-prefix', default=DEFAULT_VM_NAME_PREFIX,
                     help='prefix for virtualenv machine names')
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
    # TODO. It'll be removed after 'override' feature implementation
    parser.addoption('--resource-synchronization-test-config-file',
                     help='config file for resource synchronization test')

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
    return SimpleNamespace(
        cloud_group=request.config.getoption('--cloud-group'),
        customization=request.config.getoption('--customization'),
        work_dir=request.config.getoption('--work-dir'),
        bin_dir=request.config.getoption('--bin-dir'),
        reset_servers=not request.config.getoption('--no-servers-reset'),
        recreate_boxes=request.config.getoption('--recreate-boxes'),
        vm_name_prefix=request.config.getoption('--vm-name-prefix'),
        vm_ssh_host_config=vm_ssh_host_config,
        vm_host_work_dir=request.config.getoption('--vm-host-dir'),
        max_log_width=request.config.getoption('--max-log-width'),
        )

@pytest.fixture(scope='session')
def customization_company_name(run_options):
    return read_customization_company_name(run_options.customization)


@pytest.fixture(params=['http', 'https'])
def http_schema(request):
    return request.param


@pytest.fixture
def box(customization_company_name):
    return BoxConfigFactory(customization_company_name)

@pytest.fixture
def server(box):
    return ServerConfigFactory(box)


# cloud host dns name, like: 'cloud-dev.hdw.mx'
@pytest.fixture
def cloud_host_host(run_options):
    return resolve_cloud_host_from_registry(run_options.cloud_group, run_options.customization)

# CloudHost instance    
@pytest.fixture
def cloud_host(run_options, cloud_host_host):
    cloud_host = create_cloud_host(run_options.cloud_group, run_options.customization, cloud_host_host)
    cloud_host.check_is_ready_for_tests()
    return cloud_host


@pytest.fixture
def camera():
    return Camera()


@pytest.fixture
def sample_media_file(run_options):
    fpath = os.path.abspath(os.path.join(run_options.bin_dir, MEDIA_SAMPLE_FPATH))
    assert os.path.isfile(fpath), '%s is expected at %s' % (os.path.basename(fpath), os.path.dirname(fpath))
    return SampleMediaFile(fpath)


@pytest.fixture(scope='session')
def test_session(run_options):
    #format = '%(asctime)-15s %(threadName)s %(name)s %(levelname)s  %(message)s'
    # %.10s limits formatted string to 10 chars; %(text).10s makes the same for dict-style formatting
    format = '%%(asctime)-15s %%(threadName)-15s %%(levelname)-7s %%(message).%ds' % run_options.max_log_width
    logging.basicConfig(level=logging.DEBUG, format=format)
    session = TestSession(run_options.recreate_boxes)
    session.init(run_options)
    return session


@pytest.fixture
def env_builder(request, test_session, run_options, cloud_host_host, customization_company_name):
    return EnvironmentBuilder(request.module, test_session, run_options, request.config.cache, cloud_host_host, customization_company_name)
