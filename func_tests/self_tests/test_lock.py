import logging
import time
from multiprocessing import Process
from threading import Thread
from time import sleep

import pytest

from framework.lock import AlreadyAcquired, LocalPosixFileLock, PosixMoveLock, PosixShellFileLock
from framework.os_access.local_shell import local_shell

_logger = logging.getLogger(__name__)


@pytest.fixture(params=['move', 'local_flock', 'shell_flock'])
def lock(request, node_dir):
    return {
        'move': lambda: PosixMoveLock(local_shell, node_dir / 'move.lock'),
        'local_flock': lambda: LocalPosixFileLock(node_dir / 'local_flock.lock'),
        'shell_flock': lambda: PosixShellFileLock(local_shell, node_dir / 'shell_flock.lock'),
        }[request.param]()


def test_already_acquired_timed_out(lock):
    with lock.acquired():
        with pytest.raises(AlreadyAcquired):
            with lock.acquired(timeout_sec=2):
                pass


def test_already_acquired_wait_successfully(lock):
    exceptions = []

    def acquire_wait_release():
        try:
            with lock.acquired(timeout_sec=2):
                sleep(1)
        except Exception as e:
            _logger.exception()
            exceptions.append(e)

    threads = [
        Thread(target=acquire_wait_release),
        Thread(target=acquire_wait_release),
        ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(timeout=5)
    for exception in exceptions:
        raise exception


def _hold_lock(lock, sleep_sec):
    with lock.acquired():
        time.sleep(sleep_sec)


def test_cleanup_on_process_termination(lock):
    if isinstance(lock, PosixMoveLock):
        pytest.xfail("PosixMoveLock holds if process is terminated")
    p = Process(target=_hold_lock, args=(lock, 2))
    p.start()
    time.sleep(1)
    p.terminate()
    with lock.acquired(timeout_sec=1):
        pass
