import logging

import pytest
from pathlib2 import Path

from framework.artifact import ArtifactFactory
from framework.ca import CA
from framework.config import SingleTestConfig, TestParameter, TestsConfig
from framework.metrics_saver import MetricsSaver

pytest_plugins = ['fixtures.vms', 'fixtures.mediaservers', 'fixtures.cloud', 'fixtures.layouts', 'fixtures.media']

JUNK_SHOP_PLUGIN_NAME = 'junk-shop-db-capture'

DEFAULT_WORK_DIR = '/tmp/funtest'
DEFAULT_REST_API_FORWARDED_PORT_BASE = 17000

DEFAULT_MAX_LOG_WIDTH = 500

_logger = logging.getLogger(__name__)


def pytest_addoption(parser):
    parser.addoption('--work-dir', type=Path, default=DEFAULT_WORK_DIR, help=(
        'working directory for tests: all generated files will be placed there'))
    parser.addoption('--bin-dir', type=Path, help=(
        'directory with binary files for tests: '
        'debian distributive and media sample are expected there'))
    parser.addoption('--customization', help=(
        "Manufacturer name or 'default'. Optional. Checked against customization from .deb file."))
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
def work_dir(request):
    work_dir = request.config.getoption('--work-dir').expanduser()
    work_dir.mkdir(exist_ok=True, parents=True)
    return work_dir


@pytest.fixture(scope='session')
def bin_dir(request):
    bin_dir = request.config.getoption('--bin-dir')
    assert bin_dir, 'Argument --bin-dir is required'
    return bin_dir.expanduser()


@pytest.fixture(scope='session')
def ca(work_dir):
    return CA(work_dir / 'ca')


@pytest.fixture(scope='session', autouse=True)
def init_logging(request, work_dir):
    root_logger = logging.getLogger()
    root_logger.setLevel(logging.DEBUG)  # It's WARNING by default. Level constraints are set in handlers.

    logging.getLogger('SMB.SMBConnection').setLevel(logging.INFO)  # Big files cause too many logs.

    stderr_log_width = request.config.getoption('--max-log-width')
    stderr_handler = logging.StreamHandler()
    # %(message).10s truncates log message to 10 characters.
    stderr_handler.setFormatter(logging.Formatter(
        '%(asctime)-15s %(name)s %(levelname)s %(message).{:d}s'.format(stderr_log_width)))
    stderr_handler.setLevel(request.config.getoption('--log-level'))
    root_logger.addHandler(stderr_handler)

    file_formatter = logging.Formatter('%(asctime)s %(name)s %(levelname)s %(message)s')
    for level in {logging.DEBUG, logging.INFO}:
        file_name = logging.getLevelName(level).lower() + '.log'
        file_handler = logging.FileHandler(str(work_dir / file_name), mode='w')
        file_handler.setLevel(level)
        file_handler.setFormatter(file_formatter)
        root_logger.addHandler(file_handler)


@pytest.fixture(scope='session')
def tests_config(request):
    return TestsConfig.merge_config_list(
        request.config.getoption('--tests-config-file'),
        request.config.getoption('--test-parameters'))


@pytest.fixture()
def test_config(request, tests_config):
    if tests_config is None:
        return SingleTestConfig()
    else:
        return tests_config.get_test_config(request.node.nodeid)


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
        _logger.warning('Junk shop plugin is not available; No metrics will be saved')
    return MetricsSaver(db_capture_repository, current_test_run)
