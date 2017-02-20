import sys
import os.path
import logging
import pytest
from utils import SimpleNamespace
from test_utils import ServerFactory, EnvironmentBuilder
from vagrant_box_config import box_config_factory
from server_rest_api import CloudRestApi
from camera import MEDIA_SAMPLE_FPATH, SampleMediaFile, Camera
from server import MEDIASERVER_DEFAULT_CLOUDHOST


DEFAULT_WORK_DIR = os.path.expanduser('/tmp/funtest')
DEFAULT_BIN_DIR = os.path.expanduser('/tmp/binaries')

CLOUD_USER_NAME = 'anikitin@networkoptix.com'
CLOUD_USER_PASSWORD ='qweasd123'

log = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption('--work-dir', default=DEFAULT_WORK_DIR,
                     help='working directory for tests: all generated files will be placed there')
    parser.addoption('--bin-dir', default=DEFAULT_BIN_DIR,
                     help='directory with binary files for tests: debian distributive and sample.mkv are expected there')
    parser.addoption('--no-servers-reset', action='store_true',
                     help='skip servers reset/cleanup on test setup')
    parser.addoption('--recreate-boxes', action='store_true',
                     help='destroy and create again vagrant boxes')

@pytest.fixture
def run_options(request):
    return SimpleNamespace(
        work_dir=request.config.getoption('--work-dir'),
        bin_dir=request.config.getoption('--bin-dir'),
        reset_servers=not request.config.getoption('--no-servers-reset'),
        recreate_boxes=request.config.getoption('--recreate-boxes'),
        )


@pytest.fixture(params=['http', 'https'])
def http_schema(request):
    return request.param


@pytest.fixture
def box():
    return box_config_factory

@pytest.fixture
def server():
    return ServerFactory()


@pytest.fixture
def cloud_host_rest_api():
    return CloudRestApi('cloud', 'http://%s/' % MEDIASERVER_DEFAULT_CLOUDHOST, CLOUD_USER_NAME, CLOUD_USER_PASSWORD)

@pytest.fixture
def camera():
    return Camera()


@pytest.fixture
def sample_media_file(run_options):
    fpath = os.path.abspath(os.path.join(run_options.bin_dir, MEDIA_SAMPLE_FPATH))
    assert os.path.isfile(fpath), '%s is expected at %s' % (os.path.basename(fpath), os.path.dirname(fpath))
    return SampleMediaFile(fpath)


class TestSession(object):

    def __init__(self):
        self.boxes_recreated = False


@pytest.fixture(scope='session')
def test_session():
    #format = '%(asctime)-15s %(threadName)s %(name)s %(levelname)s  %(message)s'
    format = '%(asctime)-15s %(levelname)-7s %(message)s'
    logging.basicConfig(level=logging.DEBUG, format=format)
    return TestSession()


@pytest.fixture
def env_builder(request, test_session, run_options, cloud_host_rest_api):
    return EnvironmentBuilder(test_session, run_options, request.config.cache, cloud_host_rest_api)
