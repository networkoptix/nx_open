import pytest
from pathlib2 import Path

from test_utils.os_access import LocalAccess
from test_utils.move_lock import MoveLock


@pytest.fixture(scope='session')
def os_access():
    return LocalAccess()


@pytest.fixture()
def path(acquired, os_access):
    path = Path.home() / '.func_tests_dummy' / 'oi.lock'
    if acquired:
        os_access.run_command(['touch', path])
    else:
        os_access.run_command(['rm', '-f', path])
    return path


@pytest.fixture()
def lock(os_access, path):
    return MoveLock(os_access, path)


@pytest.mark.parametrize('acquired', [False])
def test_acquire_free(lock):
    lock.acquire()


@pytest.mark.parametrize('acquired', [True])
def test_acquire_acquired(lock):
    with pytest.raises(MoveLock.AlreadyAcquired):
        lock.acquire()


@pytest.mark.parametrize('acquired', [True])
def test_release_acquired(lock):
    lock.release()


@pytest.mark.parametrize('acquired', [False])
def test_release_free(lock):
    with pytest.raises(MoveLock.NotAcquired):
        lock.release()
