import os.path
import pytest
from utils import SimpleNamespace
from test_utils import ServerFactory, EnvironmentBuilder
from camera import MEDIA_SAMPLE_FPATH, SampleMediaFile, Camera


DEFAULT_WORK_DIR = os.path.expanduser('/tmp/funtest')


def pytest_addoption(parser):
    parser.addoption('--no-server-reset', action='store_true',
                     help='skip server reset/cleanup on test setup')
    parser.addoption('--work-dir', default=DEFAULT_WORK_DIR,
                     help='working directory for tests: all generated files will be placed there')

@pytest.fixture
def run_options(request):
    return SimpleNamespace(
        reset_server=not request.config.getoption('--no-server-reset'),
        work_dir=request.config.getoption('--work-dir'),
        )


@pytest.fixture
def server():
    print
    print 'server'
    return ServerFactory()


@pytest.fixture
def camera():
    return Camera()


@pytest.fixture
def sample_media_file():
    return SampleMediaFile(MEDIA_SAMPLE_FPATH)


@pytest.fixture
def env_builder(request, run_options):
    return EnvironmentBuilder(request.config.cache, run_options.work_dir, run_options.reset_server)
