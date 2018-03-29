import pytest
from pathlib2 import Path

from framework.os_access import LocalAccess
from framework.move_lock import MoveLock


@pytest.fixture(scope='session')
def os_access():
    return LocalAccess()


@pytest.fixture()
def path(os_access):
    path = Path.home() / '.func_tests_dummy' / 'oi.lock'
    os_access.run_command(['rm', '-f', path])
    return path


@pytest.fixture()
def lock(os_access, path):
    return MoveLock(os_access, path)


@pytest.fixture()
def same_path_lock(os_access, path):
    return MoveLock(os_access, path)


def test_already_acquired(lock, same_path_lock):
    with lock:
        with pytest.raises(MoveLock.AlreadyAcquired):
            with same_path_lock:
                pass


def test_file_present(lock, os_access, path):
    with lock:
        assert os_access.file_exists(path)


def test_not_acquired(lock, os_access, path):
    with pytest.raises(MoveLock.NotAcquired):
        with lock:
            os_access.run_command(['rm', path])
