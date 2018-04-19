from pathlib2 import Path, PurePosixPath

from framework.remote_daemon import RemoteDaemon
from framework.remote_python import RemotePython
from framework.waiting import ensure_persistence, wait_for_true

LOCAL_DIR = Path(__file__).parent / 'framework/updates2/test_updates_server'
REMOTE_DIR = PurePosixPath('/opt/mock_updates_server')
UTF_LOCALE_ENV = {'LC_ALL': 'C.UTF-8', 'LANG': 'C.UTF-8'}  # Click requires that.
ROOT_URL = 'http://localhost:8080'


def prepare_virtual_environment(os_access):
    python_installation = RemotePython(os_access)
    python_installation.install()
    return python_installation.make_venv(REMOTE_DIR / 'env')


def install_updates_server(os_access, python_path):
    daemon = RemoteDaemon('mock_updates_server', [python_path, REMOTE_DIR / 'server.py', 'serve'], env=UTF_LOCALE_ENV)
    if daemon.status(os_access) != 'started':
        remote_dir = os_access.Path(REMOTE_DIR)
        remote_dir.joinpath('server.py').upload(LOCAL_DIR / 'server.py')
        remote_dir.joinpath('requirements.txt').upload(LOCAL_DIR / 'requirements.txt')
        os_access.run_command([python_path, '-m', 'pip', 'install', '-r', remote_dir / 'requirements.txt'])
        os_access.run_command([python_path, remote_dir / 'server.py', 'generate'], env=UTF_LOCALE_ENV)
        daemon.start(os_access)
    updates_json_url = ROOT_URL + '/updates.json'
    wait_for_true(
        lambda: os_access.run_command(['curl', '-I', updates_json_url]),
        "{} is reachable".format(updates_json_url))


def test_running_linux_mediaserver(running_linux_mediaserver):
    api = running_linux_mediaserver.api.api.updates2
    ensure_persistence(lambda: api.status.GET()['status'] in {'notAvailable', 'checking'}, None)
    python_path = prepare_virtual_environment(running_linux_mediaserver.machine.os_access)
    install_updates_server(running_linux_mediaserver.machine.os_access, python_path)
    running_linux_mediaserver.stop(already_stopped_ok=True)
    running_linux_mediaserver.installation.update_mediaserver_conf({'checkForUpdateUrl': ROOT_URL})
    running_linux_mediaserver.start(already_started_ok=False)
    wait_for_true(
        lambda: api.status.GET()['status'] == 'available',
        "{} reports update is available".format(running_linux_mediaserver))
    assert not running_linux_mediaserver.installation.list_core_dumps()
