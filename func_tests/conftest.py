import errno
import logging
import logging.config
import mimetypes
import sys
from datetime import datetime
from multiprocessing.dummy import Pool as ThreadPool

import pytest
import yaml
from pathlib2 import Path
from six.moves import shlex_quote

from defaults import defaults
from framework.artifact import Artifact, ArtifactFactory, ArtifactType
from framework.ca import CA
from framework.config import SingleTestConfig, TestsConfig
from framework.metrics_saver import MetricsSaver
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.local_path import LocalPath

pytest_plugins = [
    'fixtures.vms',
    'fixtures.mediaservers',
    'fixtures.cloud',
    'fixtures.layouts',
    'fixtures.media',
    'fixtures.backward_compatibility',
    ]

JUNK_SHOP_PLUGIN_NAME = 'junk-shop-db-capture'

_logger = logging.getLogger(__name__)


@pytest.hookimpl(hookwrapper=True)
def pytest_pyfunc_call(pyfuncitem):
    outcome = yield
    if outcome.excinfo is not None and 'skipifnotimplemented' in pyfuncitem.keywords:
        _, e, _ = outcome.excinfo
        if isinstance(e, NotImplementedError):
            pytest.skip(str(e))


@pytest.mark.optionalhook
def pytest_metadata(metadata):
    command_line = ' '.join(map(shlex_quote, sys.argv))
    metadata["Command Line"] = command_line


def pytest_addoption(parser):
    parser.addoption(
        '--work-dir',
        default=defaults.get('work_dir'),
        help="Almost all files generated by tests placed there.")
    parser.addoption(
        '--bin-dir',
        default=defaults.get('bin_dir'),
        help="Media samples and other files required by tests are expected there.")
    parser.addoption(
        '--logging-config',
        default=defaults.get('logging_config'),
        help="Configuration file for logging, in yaml format. Relative to logging-config dir if relative.")
    parser.addoption(
        '--tests-config-file',
        nargs='*',
        help="Configuration file(s) for tests, in yaml format.")
    parser.addoption(
        '--test-parameters',
        help="Configuration parameters for a test, format: --test-parameter=test.param1=value1,test.param2=value2.")
    parser.addoption(
        '--clean', '--reinstall',
        action='store_true',
        help="Destroy VMs first.")
    parser.addoption(
        '--slot', '-S', type=int, default=defaults.get('slot', 0),
        help=(
            "Small non-negative integer used to calculate forwarded port numbers and included into VM names. "
            "Runs with different slots share nothing. "
            "Slots allocation is user's responsibility by design. "
            "Number of slots depends mostly on port forwarding configuration. "
            "It's still possible to run tests in parallel within same slot, but such runs would share VMs."))


@pytest.fixture()
def slot(request, metadata):
    slot = request.config.getoption('--slot')
    metadata['Slot'] = slot
    return slot


@pytest.fixture()
def service_ports(slot):
    begin = 40000 + 100 * slot
    ports = range(begin, begin + 100)
    return ports


@pytest.fixture(scope='session')
def _work_dir(request, metadata):
    """Make dir once but return a simple type to allow serialization."""
    work_dir = LocalPath(request.config.getoption('--work-dir')).expanduser()
    # Don't create parents to fail fast if work dir is misconfigured.
    work_dir.mkdir(exist_ok=True, parents=False)
    metadata['Work Dir'] = work_dir
    return work_dir.parts


@pytest.fixture()
def work_dir(_work_dir):
    return LocalPath(*_work_dir)


@pytest.fixture(scope='session')
def run_id(metadata):
    run_id = '{:%Y%m%d_%H%M%S}'.format(datetime.now())
    metadata['Run ID'] = run_id
    return run_id


@pytest.fixture(scope='session')
def _run_dir(metadata, _work_dir, run_id):
    """Make dir and symlink once but return a simple type to allow serialization."""
    prefix = 'run_'
    work_dir = LocalPath(*_work_dir)
    this = work_dir / (prefix + run_id)
    metadata['Run Dir'] = this
    this.mkdir(parents=False, exist_ok=True)
    latest = work_dir / 'latest'
    try:
        latest.unlink()
    except DoesNotExist:
        pass
    try:
        latest.symlink_to(this, target_is_directory=True)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise
    old = list(sorted(work_dir.glob('{}*'.format(prefix))))[:-5]
    pool = ThreadPool(10)  # Arbitrary, just not so much.
    future = pool.map_async(lambda dir: dir.rmtree(), old)
    yield this.parts
    future.wait(timeout=30)
    pool.terminate()


@pytest.fixture()
def run_dir(_run_dir):
    return LocalPath(*_run_dir)


@pytest.fixture()
def node_dir(request, run_dir):
    # Don't call it "test_dir" to avoid interpretation as test.
    # `node`, in pytest terms, is test with instantiated parameters.
    node_dir = run_dir.joinpath(*request.node.listnames()[1:])  # First path is always same.
    node_dir.mkdir(parents=True, exist_ok=False)
    return node_dir


# TODO: Find out whether they exist on all supports OSes.
mimetypes.add_type('application/vnd.tcpdump.pcap', '.cap')
mimetypes.add_type('application/vnd.tcpdump.pcap', '.pcap')
mimetypes.add_type('text/plain', '.log')
mimetypes.add_type('application/x-yaml', '.yaml')
mimetypes.add_type('application/x-yaml', '.yml')


@pytest.fixture()
def artifacts_dir(node_dir, artifact_set):
    dir = node_dir / 'artifacts'
    dir.mkdir(exist_ok=True)
    yield dir
    for entry in dir.walk():
        # noinspection PyUnresolvedReferences
        mime_type = mimetypes.types_map.get(entry.suffix, 'application/octet-stream')
        type = ArtifactType(entry.suffix[1:] if entry.suffix else 'unknown_type', mime_type, ext=entry.suffix)
        name = str(entry.relative_to(dir))
        is_error = any(word in entry.name for word in {'core', 'backtrace'})
        artifact_set.add(Artifact(entry, name=name, is_error=is_error, artifact_type=type))


@pytest.fixture()
def bin_dir(request):
    bin_dir = LocalPath(request.config.getoption('--bin-dir'))
    assert bin_dir, 'Argument --bin-dir is required'
    return bin_dir.expanduser()


@pytest.fixture()
def ca(request, work_dir):
    """CA key pair, persistent between runs -- cert can be added to browser."""
    return CA(work_dir / 'ca', clean=request.config.getoption('--clean'))


@pytest.fixture(autouse=True)
def init_logging(request, run_dir):
    logging_config_path = request.config.getoption('--logging-config')
    if logging_config_path:
        full_path = Path(__file__).parent / 'logging-config' / logging_config_path
        config_text = full_path.read_text()
        config = yaml.load(config_text)
        logging.config.dictConfig(config)
        logging.info('Logging is initialized from "%s".', full_path)

    root_logger = logging.getLogger()
    file_formatter = logging.Formatter('%(asctime)s %(name)s %(levelname)s %(message)s')
    for level in {logging.DEBUG, logging.INFO}:
        file_name = logging.getLevelName(level).lower() + '.log'
        file_handler = logging.FileHandler(str(run_dir / file_name), mode='w')
        file_handler.setLevel(level)
        file_handler.setFormatter(file_formatter)
        root_logger.addHandler(file_handler)


@pytest.fixture()
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
def artifact_set(junk_shop_repository):
    artifact_set = set()
    yield artifact_set
    repository, current_test_run = junk_shop_repository
    if repository:
        for artifact in sorted(artifact_set, key=lambda artifact: artifact.name):
            assert artifact.artifact_type, repr(artifact)
            _logger.info('Storing artifact: %r', artifact)
            if not artifact.path.exists():
                _logger.warning('Artifact file is missing, skipping: %s' % artifact.path)
                continue
            data = artifact.path.read_bytes()
            repository_artifact_type = artifact.artifact_type.produce_repository_type(repository)
            _logger.info("Save artifact: %r", artifact)
            repository.add_artifact_with_session(
                current_test_run,
                artifact.name,
                artifact.full_name or artifact.name,
                repository_artifact_type,
                data,
                artifact.is_error or False,
                )


@pytest.fixture()
def artifact_factory(node_dir, artifact_set):
    return ArtifactFactory.from_path(artifact_set, node_dir)


@pytest.fixture()
def metrics_saver(junk_shop_repository):
    db_capture_repository, current_test_run = junk_shop_repository
    if not db_capture_repository:
        _logger.warning('Junk shop plugin is not available; No metrics will be saved')
    return MetricsSaver(db_capture_repository, current_test_run)
