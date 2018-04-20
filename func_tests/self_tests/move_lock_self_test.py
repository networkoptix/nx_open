import pytest

from framework.move_lock import MoveLock, MoveLockAlreadyAcquired, MoveLockNotAcquired

pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.fixture()
def path(ad_hoc_ssh_access):
    path = ad_hoc_ssh_access.Path('/tmp/func_tests/move_lock_sandbox/oi.lock')
    path.parent.mkdir(exist_ok=True, parents=True)
    if path.exists():
        path.unlink()
    return path


@pytest.fixture()
def lock(ad_hoc_ssh_access, path):
    return MoveLock(ad_hoc_ssh_access, path)


@pytest.fixture()
def same_path_lock(ad_hoc_ssh_access, path):
    return MoveLock(ad_hoc_ssh_access, path)


def test_already_acquired(lock, same_path_lock):
    with lock:
        with pytest.raises(MoveLockAlreadyAcquired):
            with same_path_lock:
                pass


def test_file_present(lock, path):
    with lock:
        assert path.exists()


def test_not_acquired(lock, path):
    with pytest.raises(MoveLockNotAcquired):
        with lock:
            path.unlink()
