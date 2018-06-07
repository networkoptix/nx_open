from pathlib2 import Path, PurePosixPath

from framework.os_access.path import copy_file
from framework.remote_daemon import RemoteDaemon
from framework.remote_python import RemotePython
from framework.waiting import ensure_persistence, wait_for_true

LOCAL_DIR = Path(__file__).parent / 'updates_server'
REMOTE_DIR = PurePosixPath('/opt/mock_updates_server')
UTF_LOCALE_ENV = {'LC_ALL': 'C.UTF-8', 'LANG': 'C.UTF-8'}  # Click requires that.
ROOT_URL = 'http://localhost:8080'


def prepare_virtual_environment(ssh_access):
    python_installation = RemotePython(ssh_access)
    python_installation.install()
    return python_installation.make_venv(REMOTE_DIR / 'env')


def install_updates_server(ssh_access, python_path):
    daemon = RemoteDaemon('mock_updates_server', [python_path, REMOTE_DIR / 'server.py', 'serve'], env=UTF_LOCALE_ENV)
    if daemon.status(ssh_access) != 'started':
        remote_dir = ssh_access.Path(REMOTE_DIR)
        copy_file(LOCAL_DIR / 'server.py', remote_dir / 'server.py')
        copy_file(LOCAL_DIR / 'requirements.txt', remote_dir / 'requirements.txt')
        ssh_access.run_command([python_path, '-m', 'pip', 'install', '-r', remote_dir / 'requirements.txt'])
        ssh_access.run_command([python_path, remote_dir / 'server.py', 'generate'], env=UTF_LOCALE_ENV)
        daemon.start(ssh_access)
    updates_json_url = ROOT_URL + '/updates.json'
    wait_for_true(
        lambda: ssh_access.run_command(['curl', '-I', updates_json_url]),
        "{} is reachable".format(updates_json_url))


def test_running_linux_mediaserver(one_running_mediaserver):
    api = one_running_mediaserver.api.api.updates2
    ensure_persistence(lambda: api.status.GET()['status'] in {'notAvailable', 'checking'}, None)
    python_path = prepare_virtual_environment(one_running_mediaserver.machine.os_access)
    install_updates_server(one_running_mediaserver.machine.os_access, python_path)
    one_running_mediaserver.stop(already_stopped_ok=True)
    one_running_mediaserver.installation.update_mediaserver_conf({'checkForUpdateUrl': ROOT_URL})
    one_running_mediaserver.start(already_started_ok=False)
    wait_for_true(
        lambda: api.status.GET()['status'] == 'available',
        "{} reports update is available".format(one_running_mediaserver))
    assert not one_running_mediaserver.installation.list_core_dumps()
