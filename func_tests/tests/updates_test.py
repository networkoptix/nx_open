from pathlib2 import Path

from framework.remote_daemon import RemoteDaemon
from framework.remote_python import RemotePython
from framework.utils import holds_long_enough, wait_until

LOCAL_DIR = Path(__file__).parent / 'framework/updates2/test_updates_server'
REMOTE_DIR = Path('/opt/mock_updates_server')
UTF_LOCALE_ENV = {'LC_ALL': 'C.UTF-8', 'LANG': 'C.UTF-8'}  # Click requires that.
ROOT_URL = 'http://localhost:8080'


def prepare_virtual_environment(os_access):
    python_installation = RemotePython(os_access)
    python_installation.install()
    return python_installation.make_venv(REMOTE_DIR / 'env')


def install_updates_server(os_access, python_path):
    daemon = RemoteDaemon('mock_updates_server', [python_path, REMOTE_DIR / 'server.py', 'serve'], env=UTF_LOCALE_ENV)
    if daemon.status(os_access) != 'started':
        os_access.put_file(LOCAL_DIR / 'server.py', REMOTE_DIR / 'server.py')
        os_access.put_file(LOCAL_DIR / 'requirements.txt', REMOTE_DIR / 'requirements.txt')
        os_access.run_command([python_path, '-m', 'pip', 'install', '-r', REMOTE_DIR / 'requirements.txt'])
        os_access.run_command([python_path, REMOTE_DIR / 'server.py', 'generate'], env=UTF_LOCALE_ENV)
        daemon.start(os_access)
    wait_until(lambda: os_access.run_command(['curl', '-I', ROOT_URL + '/updates.json']))


def test_running_linux_mediaserver(running_linux_mediaserver):
    api = running_linux_mediaserver.api.api.updates2
    assert holds_long_enough(lambda: api.status.GET()['status'] in {'notAvailable', 'checking'})
    python_path = prepare_virtual_environment(running_linux_mediaserver.machine.os_access)
    install_updates_server(running_linux_mediaserver.machine.os_access, python_path)
    running_linux_mediaserver.stop(already_stopped_ok=True)
    running_linux_mediaserver.installation.update_mediaserver_conf({'checkForUpdateUrl': ROOT_URL})
    running_linux_mediaserver.start(already_started_ok=False)
    assert wait_until(lambda: api.status.GET()['status'] == 'available')
    assert not running_linux_mediaserver.installation.list_core_dumps()
