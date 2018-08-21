import logging
import time
from multiprocessing import Process, Condition
from threading import Thread, current_thread
from time import sleep

import pytest

from framework.lock import AlreadyAcquired, PosixShellFileLock, PortalockerLock
from framework.os_access.local_shell import local_shell

_logger = logging.getLogger(__name__)


@pytest.fixture(params=['move', 'local_flock', 'shell_flock', 'portalocker'])
def lock(request, node_dir):
    return {
        'shell_flock': lambda: PosixShellFileLock(local_shell, node_dir / 'shell_flock.lock'),
        'portalocker': lambda: PortalockerLock(node_dir / 'shell_flock.lock'),
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
            _logger.exception("Exception in %r.", current_thread())
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


def _hold_lock(lock, cv):
    cv.acquire()
    with lock.acquired():
        cv.notify()
        cv.wait()


def test_cleanup_on_process_termination(lock):
    if isinstance(lock, PosixMoveLock):
        pytest.xfail("PosixMoveLock holds if process is terminated")
    cv = Condition()
    cv.acquire()
    p = Process(target=_hold_lock, args=(lock, cv))
    p.start()
    cv.wait()
    p.terminate()
    with lock.acquired(timeout_sec=1):
        pass
