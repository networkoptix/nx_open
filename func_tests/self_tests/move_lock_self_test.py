from threading import Thread
from time import sleep

import pytest

from framework.move_lock import MoveLock, MoveLockAlreadyAcquired, MoveLockNotAcquired

pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.fixture()
def path(ad_hoc_ssh):
    path = ad_hoc_ssh.Path('/tmp/func_tests/move_lock_sandbox/oi.lock')
    path.parent.mkdir(exist_ok=True, parents=True)
    if path.exists():
        path.unlink()
    return path


@pytest.fixture()
def lock(ad_hoc_ssh, path):
    return MoveLock(ad_hoc_ssh, path, timeout_sec=2)


@pytest.fixture()
def same_path_lock(ad_hoc_ssh, path):
    return MoveLock(ad_hoc_ssh, path, timeout_sec=2)


def test_already_acquired_timed_out(lock, same_path_lock):
    with lock:
        with pytest.raises(MoveLockAlreadyAcquired):
            with same_path_lock:
                pass


def test_already_acquired_wait_successfully(lock, same_path_lock):
    exceptions = {}

    def acquire_wait_release(some_lock, exception_key):
        try:
            with some_lock:
                sleep(1)
        except Exception as e:
            exceptions[exception_key] = e

    threads = [
        Thread(target=acquire_wait_release, args=[lock, 'lock']),
        Thread(target=acquire_wait_release, args=[same_path_lock, 'same_path_lock']),
        ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(timeout=5)
    for exception in exceptions:
        raise exception


def test_file_present(lock, path):
    with lock:
        assert path.exists()


def test_not_acquired(lock, path):
    with pytest.raises(MoveLockNotAcquired):
        with lock:
            path.unlink()
